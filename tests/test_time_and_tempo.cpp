#include <saudade/time/time_types.hpp>
#include <saudade/time/tempo_map.hpp>

#include <cassert>
#include <cmath>
#include <iostream>

using namespace saudade::time;

void test_beat_duration_and_position_types() {
    std::cout << "[RUN] test_beat_duration_and_position_types\n";

    // 1. Creation from whole beats
    const BeatPosition b0 = BeatPosition::zero();
    assert(b0.ticks == 0);
    assert(b0.to_double() == 0.0);

    const BeatPosition b1 = BeatPosition::from_beats(1);
    assert(b1.ticks == BeatPosition::kTicksPerBeat);
    assert(b1.to_double() == 1.0);

    const BeatPosition b4 = BeatPosition::from_beats(4);
    assert(b4.ticks == 4 * BeatPosition::kTicksPerBeat);
    assert(b4.to_double() == 4.0);

    // 2. Fractions: 1/2 (eighth note), 1/4 (sixteenth note), 1/3 (triplet)
    const BeatDuration half = BeatDuration::from_fraction(1, 2);
    assert(half.ticks == BeatPosition::kTicksPerBeat / 2);
    assert(half.to_double() == 0.5);

    const BeatDuration quarter = BeatDuration::from_fraction(1, 4);
    assert(quarter.ticks == BeatPosition::kTicksPerBeat / 4);
    assert(quarter.to_double() == 0.25);

    const BeatDuration triplet = BeatDuration::from_fraction(1, 3);
    assert(triplet.ticks == BeatPosition::kTicksPerBeat / 3);
    assert(std::abs(triplet.to_double() - (1.0 / 3.0)) < 1e-6);

    // Exact divisibility check: 3 * triplet == 1 beat
    assert((triplet + triplet + triplet).ticks == BeatPosition::kTicksPerBeat);

    // 3. Arithmetic operations
    BeatPosition p = b1 + half;
    assert(p.ticks == BeatPosition::kTicksPerBeat + BeatPosition::kTicksPerBeat / 2);
    assert(p.to_double() == 1.5);

    p -= quarter;
    assert(p.to_double() == 1.25);

    const BeatDuration diff = p - b1;
    assert(diff.to_double() == 0.25);

    // 4. Comparison operators
    assert(b0 < b1);
    assert(b4 > b1);
    assert(b1 == BeatPosition::from_ticks(BeatPosition::kTicksPerBeat));
    assert(b0 <= b1);
    assert(b4 >= b1);
    assert(b1 != b4);

    std::cout << "  [PASS] Beat types, fractions, and operators verified\n";
}

void test_tempo_map_conversions_48k() {
    std::cout << "[RUN] test_tempo_map_conversions_48k\n";

    const TempoMap tempo(120.0); // 120 BPM: 1 beat = 0.5s
    const double sr = 48000.0;

    // Beat 0 -> Sample 0
    assert(tempo.beat_to_sample(BeatPosition::zero(), sr) == 0);
    assert(tempo.sample_to_beat(0, sr) == BeatPosition::zero());

    // Beat 1 -> 0.5s * 48000 = 24000 samples
    const auto s_b1 = tempo.beat_to_sample(BeatPosition::from_beats(1), sr);
    assert(s_b1 == 24000);
    assert(tempo.sample_to_beat(24000, sr) == BeatPosition::from_beats(1));

    // Beat 4 -> 2.0s * 48000 = 96000 samples
    const auto s_b4 = tempo.beat_to_sample(BeatPosition::from_beats(4), sr);
    assert(s_b4 == 96000);
    assert(tempo.sample_to_beat(96000, sr) == BeatPosition::from_beats(4));

    // Triplet (1/3 beat) -> 0.5s / 3 * 48000 = 8000 samples exactly
    const auto triplet_pos = BeatPosition::from_fraction(1, 3);
    const auto s_triplet = tempo.beat_to_sample(triplet_pos, sr);
    assert(s_triplet == 8000);
    assert(tempo.sample_to_beat(8000, sr) == triplet_pos);

    // 1/16th note (1/4 beat) -> 6000 samples exactly
    const auto sixteenth_pos = BeatPosition::from_fraction(1, 4);
    assert(tempo.beat_to_sample(sixteenth_pos, sr) == 6000);
    assert(tempo.sample_to_beat(6000, sr) == sixteenth_pos);

    std::cout << "  [PASS] Exact 48 kHz conversions at 120 BPM verified\n";
}

void test_tempo_map_conversions_44k1() {
    std::cout << "[RUN] test_tempo_map_conversions_44k1\n";

    const TempoMap tempo(120.0); // 120 BPM: 1 beat = 0.5s
    const double sr = 44100.0;

    // Beat 0 -> Sample 0
    assert(tempo.beat_to_sample(BeatPosition::zero(), sr) == 0);
    assert(tempo.sample_to_beat(0, sr) == BeatPosition::zero());

    // Beat 1 -> 0.5s * 44100 = 22050 samples
    const auto s_b1 = tempo.beat_to_sample(BeatPosition::from_beats(1), sr);
    assert(s_b1 == 22050);
    assert(tempo.sample_to_beat(22050, sr) == BeatPosition::from_beats(1));

    // Beat 4 -> 2.0s * 44100 = 88200 samples
    const auto s_b4 = tempo.beat_to_sample(BeatPosition::from_beats(4), sr);
    assert(s_b4 == 88200);
    assert(tempo.sample_to_beat(88200, sr) == BeatPosition::from_beats(4));

    // 1/2 beat (eighth) -> 11025 samples
    const auto half_pos = BeatPosition::from_fraction(1, 2);
    assert(tempo.beat_to_sample(half_pos, sr) == 11025);
    assert(tempo.sample_to_beat(11025, sr) == half_pos);

    std::cout << "  [PASS] Exact 44.1 kHz conversions at 120 BPM verified\n";
}

void test_round_trip_accuracy() {
    std::cout << "[RUN] test_round_trip_accuracy\n";

    const TempoMap tempo(120.0);

    for (double sr : {44100.0, 48000.0, 96000.0}) {
        // Test 10,000 sequential samples: round-trip error must be <= 1 sample
        for (SamplePosition s = 0; s < 10000; s += 7) {
            const BeatPosition beat = tempo.sample_to_beat(s, sr);
            const SamplePosition roundtrip = tempo.beat_to_sample(beat, sr);
            const SampleDuration err = std::abs(s - roundtrip);
            assert(err <= 1);
        }

        // Test musical beat positions up to 32 beats: round-trip error <= 1 sample
        for (int b = 0; b <= 32; ++b) {
            for (int denom : {1, 2, 3, 4, 6, 8, 12, 16}) {
                for (int num = 0; num < denom; ++num) {
                    const BeatPosition pos = BeatPosition::from_beats(b) + BeatDuration::from_fraction(num, denom);
                    const SamplePosition s = tempo.beat_to_sample(pos, sr);
                    const BeatPosition rt_beat = tempo.sample_to_beat(s, sr);
                    const SamplePosition rt_sample = tempo.beat_to_sample(rt_beat, sr);
                    assert(std::abs(s - rt_sample) <= 1);
                }
            }
        }
    }

    std::cout << "  [PASS] Round-trip discretization error bounded by <= 1 sample across 44.1, 48, and 96 kHz\n";
}

void test_variable_bpm() {
    std::cout << "[RUN] test_variable_bpm\n";

    // 60 BPM: 1 beat = 1.0 second
    const TempoMap tempo60(60.0);
    assert(tempo60.beat_to_sample(BeatPosition::from_beats(1), 48000.0) == 48000);
    assert(tempo60.beat_to_sample(BeatPosition::from_beats(1), 44100.0) == 44100);

    // 140 BPM: 1 beat = 60/140 = 3/7 second
    const TempoMap tempo140(140.0);
    const SamplePosition s140 = tempo140.beat_to_sample(BeatPosition::from_beats(1), 44100.0);
    // 44100 * 3 / 7 = 18900
    assert(s140 == 18900);
    assert(tempo140.sample_to_beat(18900, 44100.0) == BeatPosition::from_beats(1));

    std::cout << "  [PASS] Variable BPM support verified\n";
}

int main() {
    try {
        test_beat_duration_and_position_types();
        test_tempo_map_conversions_48k();
        test_tempo_map_conversions_44k1();
        test_round_trip_accuracy();
        test_variable_bpm();
        std::cout << "All time and tempo tests PASSED!\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed: " << ex.what() << "\n";
        return 1;
    }
}
