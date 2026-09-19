#include <saudade/ui/project_file.hpp>

#include <QCoreApplication>
#include <QTemporaryDir>

#include <cassert>
#include <iostream>

using namespace saudade;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    assert(directory.isValid());

    model::Project source;
    source.set_name("Roundtrip");
    assert(source.set_bpm(137.5));
    source.set_master_gain_db(-2.0f);
    auto patch = source.synth_patch();
    patch.cutoff_hz = 5400.0f;
    patch.release_seconds = 0.75f;
    source.set_synth_patch(patch);
    const auto track_id = source.add_track("Lead");
    auto* track = source.find_track(track_id);
    track->mixer.gain_db = -4.5f;
    track->mixer.pan = 0.25f;
    track->mixer.muted = true;

    const auto pattern_id = source.add_pattern(
        "Theme", time::BeatDuration::from_beats(8));
    auto* pattern = source.find_pattern(pattern_id);
    const auto lane_id = pattern->add_lane("Notes");
    auto* sequence = &pattern->find_lane(lane_id)->notes();
    const auto note_id = sequence->add_note(
        time::BeatPosition::from_fraction(1, 2),
        time::BeatDuration::from_fraction(3, 2),
        60.25, 0.72f, 0.18f);
    assert(note_id.has_value());
    const auto clip_id = source.add_clip(
        track_id, pattern_id, time::BeatPosition::from_beats(4),
        time::BeatDuration::from_beats(8));
    assert(clip_id != 0);

    const QString path = directory.filePath("roundtrip.dawproj");
    QString error;
    assert(ui::ProjectFile::save(source, path, &error));
    assert(error.isEmpty());

    model::Project loaded;
    assert(ui::ProjectFile::load(path, loaded, &error));
    assert(error.isEmpty());
    assert(loaded.name() == source.name());
    assert(loaded.bpm() == source.bpm());
    assert(loaded.master_gain_db() == -2.0f);
    assert(loaded.synth_patch().cutoff_hz == 5400.0f);
    assert(loaded.synth_patch().release_seconds == 0.75f);
    assert(loaded.tracks().size() == 1);
    assert(loaded.patterns().size() == 1);
    assert(loaded.clips().size() == 1);
    assert(loaded.tracks().front().id == track_id);
    assert(loaded.tracks().front().mixer.gain_db == -4.5f);
    assert(loaded.tracks().front().mixer.pan == 0.25f);
    assert(loaded.tracks().front().mixer.muted);
    const auto& loaded_note =
        loaded.patterns().front().lanes().front().notes().notes().front();
    assert(loaded_note.note_id == *note_id);
    assert(loaded_note.start == time::BeatPosition::from_fraction(1, 2));
    assert(loaded_note.duration == time::BeatDuration::from_fraction(3, 2));
    assert(loaded_note.pitch == 60.25);

    std::cout << "PROJECT FILE ROUNDTRIP PASSED\n";
    return 0;
}
