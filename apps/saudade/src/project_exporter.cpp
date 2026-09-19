#include <saudade/ui/project_exporter.hpp>

#include <saudade/audio/audio_buffer.hpp>
#include <saudade/audio/engine.hpp>

#include <QSaveFile>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>

namespace saudade::ui {
namespace {

void append_le16(std::array<char, 44>& header, size_t offset, uint16_t value) {
    header[offset] = static_cast<char>(value & 0xFFU);
    header[offset + 1] = static_cast<char>((value >> 8U) & 0xFFU);
}

void append_le32(std::array<char, 44>& header, size_t offset, uint32_t value) {
    header[offset] = static_cast<char>(value & 0xFFU);
    header[offset + 1] = static_cast<char>((value >> 8U) & 0xFFU);
    header[offset + 2] = static_cast<char>((value >> 16U) & 0xFFU);
    header[offset + 3] = static_cast<char>((value >> 24U) & 0xFFU);
}

bool fail(QString* error, const QString& message) {
    if (error) *error = message;
    return false;
}

} // namespace

bool ProjectExporter::render_wav(
    const renderplan::RenderPlan& plan,
    std::span<const events::TimelineEvent> timeline_events,
    double bpm,
    float master_gain_db,
    time::BeatPosition start,
    time::BeatPosition end,
    const QString& path,
    std::atomic<bool>* cancel,
    ProgressCallback progress,
    QString* error) {
    constexpr double sample_rate = 48000.0;
    constexpr uint32_t quantum = 512;
    constexpr uint16_t channels = 2;
    constexpr uint16_t bits_per_sample = 16;

    if (end <= start) {
        return fail(error, QStringLiteral("Export range is empty"));
    }
    auto offline_plan = std::make_unique<renderplan::RenderPlan>(plan);
    // EventTrackBuffer is deliberately large and preallocated for RT. Keep the
    // offline instance off the UI thread's comparatively small stack.
    auto engine = std::make_unique<audio::AudioEngine>(
        std::move(offline_plan), quantum);
    engine->transport().set_bpm(bpm);
    engine->set_master_gain_db(master_gain_db);
    engine->transport().set_loop_enabled(false);
    engine->set_track_events(std::vector<events::TimelineEvent>(
        timeline_events.begin(), timeline_events.end()));
    engine->seek_beats(start, sample_rate);
    engine->play();

    const auto start_sample =
        engine->transport().tempo_map().beat_to_sample(start, sample_rate);
    const auto end_sample =
        engine->transport().tempo_map().beat_to_sample(end, sample_rate);
    const auto frame_count_i64 = end_sample - start_sample;
    if (frame_count_i64 <= 0 ||
        static_cast<uint64_t>(frame_count_i64) >
            std::numeric_limits<uint32_t>::max() / (channels * sizeof(int16_t))) {
        return fail(error, QStringLiteral("Export range is too long for WAV"));
    }
    const uint32_t frame_count = static_cast<uint32_t>(frame_count_i64);
    const uint32_t data_bytes =
        frame_count * channels * static_cast<uint32_t>(sizeof(int16_t));

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(error, file.errorString());
    }

    std::array<char, 44> header{};
    std::memcpy(header.data(), "RIFF", 4);
    append_le32(header, 4, 36U + data_bytes);
    std::memcpy(header.data() + 8, "WAVEfmt ", 8);
    append_le32(header, 16, 16);
    append_le16(header, 20, 1);
    append_le16(header, 22, channels);
    append_le32(header, 24, static_cast<uint32_t>(sample_rate));
    append_le32(header, 28, static_cast<uint32_t>(sample_rate) *
                              channels * (bits_per_sample / 8U));
    append_le16(header, 32, channels * (bits_per_sample / 8U));
    append_le16(header, 34, bits_per_sample);
    std::memcpy(header.data() + 36, "data", 4);
    append_le32(header, 40, data_bytes);
    if (file.write(header.data(), static_cast<qint64>(header.size())) !=
        static_cast<qint64>(header.size())) {
        return fail(error, file.errorString());
    }

    audio::AudioBuffer buffer(channels, quantum);
    std::vector<int16_t> interleaved(static_cast<size_t>(quantum) * channels);
    uint32_t rendered = 0;
    while (rendered < frame_count) {
        if (cancel && cancel->load(std::memory_order_acquire)) {
            file.cancelWriting();
            return fail(error, QStringLiteral("Export cancelled"));
        }
        const uint32_t frames = std::min(quantum, frame_count - rendered);
        auto block = buffer.block(frames);
        engine->process(block, audio::ProcessContext{sample_rate, frames});
        for (uint32_t frame = 0; frame < frames; ++frame) {
            for (uint16_t channel = 0; channel < channels; ++channel) {
                const float sample = std::clamp(
                    buffer.channel(channel)[frame], -1.0f, 1.0f);
                interleaved[static_cast<size_t>(frame) * channels + channel] =
                    static_cast<int16_t>(std::lrint(sample * 32767.0f));
            }
        }
        const auto bytes = static_cast<qint64>(
            static_cast<size_t>(frames) * channels * sizeof(int16_t));
        if (file.write(reinterpret_cast<const char*>(interleaved.data()), bytes) != bytes) {
            return fail(error, file.errorString());
        }
        rendered += frames;
        if (progress) progress(static_cast<double>(rendered) / frame_count);
    }
    engine->stop();
    if (!file.commit()) {
        return fail(error, file.errorString());
    }
    if (progress) progress(1.0);
    if (error) error->clear();
    return true;
}

} // namespace saudade::ui
