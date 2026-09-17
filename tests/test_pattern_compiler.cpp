#include <saudade/model/pattern.hpp>
#include <saudade/model/pattern_compiler.hpp>
#include <saudade/time/tempo_map.hpp>
#include <saudade/events/event_types.hpp>

#include <cassert>
#include <iostream>
#include <vector>

using namespace saudade;

void test_compiler_accuracy_48k_and_44k() {
    std::cout << "[RUN] test_compiler_accuracy_48k_and_44k\n";

    time::TempoMap tempo(120.0); // 120 BPM: 1 beat = 0.5s

    model::NoteSequence seq;
    // Note 1: [0.0, 0.5 beats) -> at 48k: [0, 12000 samples)
    seq.add_note(time::BeatPosition::zero(),
                 time::BeatDuration::from_fraction(1, 2),
                 60.0, 0.8f, 0.1f);
    // Note 2: [1.0, 2.0 beats) -> at 48k: [24000, 48000 samples)
    seq.add_note(time::BeatPosition::from_beats(1),
                 time::BeatDuration::from_beats(1),
                 64.0, 0.9f, 0.2f);

    // Test at 48000 Hz
    {
        const auto events = model::PatternCompiler::compile_sequence(seq, tempo, 48000.0);
        assert(events.size() == 4);

        // Note 1 On
        assert(events[0].sample_position == 0);
        assert(std::holds_alternative<events::NoteOn>(events[0].payload));
        const auto& on1 = std::get<events::NoteOn>(events[0].payload);
        assert(on1.note_id == 1);
        assert(on1.pitch == 60.0);
        assert(on1.velocity == 0.8f);

        // Note 1 Off
        assert(events[1].sample_position == 12000);
        assert(std::holds_alternative<events::NoteOff>(events[1].payload));
        const auto& off1 = std::get<events::NoteOff>(events[1].payload);
        assert(off1.note_id == 1);
        assert(off1.release_velocity == 0.1f);

        // Note 2 On
        assert(events[2].sample_position == 24000);
        assert(std::holds_alternative<events::NoteOn>(events[2].payload));
        const auto& on2 = std::get<events::NoteOn>(events[2].payload);
        assert(on2.note_id == 2);
        assert(on2.pitch == 64.0);
        assert(on2.velocity == 0.9f);

        // Note 2 Off
        assert(events[3].sample_position == 48000);
        assert(std::holds_alternative<events::NoteOff>(events[3].payload));
        const auto& off2 = std::get<events::NoteOff>(events[3].payload);
        assert(off2.note_id == 2);
        assert(off2.release_velocity == 0.2f);
    }

    // Test at 44100 Hz
    {
        const auto events = model::PatternCompiler::compile_sequence(seq, tempo, 44100.0);
        assert(events.size() == 4);

        // 44100 Hz at 120 BPM:
        // 0.5 beat = 0.25 s = 11025 samples
        // 1.0 beat = 0.50 s = 22050 samples
        // 2.0 beats = 1.00 s = 44100 samples
        assert(events[0].sample_position == 0);
        assert(events[1].sample_position == 11025);
        assert(events[2].sample_position == 22050);
        assert(events[3].sample_position == 44100);
    }

    // Test with pattern_start_beat = 2.0 (1.0 second offset = 48000 samples at 48k)
    {
        const auto events = model::PatternCompiler::compile_sequence(
            seq, tempo, 48000.0, time::BeatPosition::from_beats(2));
        assert(events.size() == 4);
        assert(events[0].sample_position == 48000);
        assert(events[1].sample_position == 60000);
        assert(events[2].sample_position == 72000);
        assert(events[3].sample_position == 96000);
    }

    std::cout << "[PASS] test_compiler_accuracy_48k_and_44k\n";
}

void test_legato_retrigger_boundary_ordering() {
    std::cout << "[RUN] test_legato_retrigger_boundary_ordering\n";

    time::TempoMap tempo(120.0);
    model::NoteSequence seq;

    // Note A: [0.0, 1.0) -> samples [0, 24000)
    seq.add_note(time::BeatPosition::zero(), time::BeatDuration::from_beats(1), 60.0);
    // Note B: [1.0, 2.0) -> samples [24000, 48000)
    seq.add_note(time::BeatPosition::from_beats(1), time::BeatDuration::from_beats(1), 64.0);

    const auto events = model::PatternCompiler::compile_sequence(seq, tempo, 48000.0);
    assert(events.size() == 4);

    // Event 0: NoteOn A at sample 0
    assert(events[0].sample_position == 0);
    assert(std::holds_alternative<events::NoteOn>(events[0].payload));
    assert(std::get<events::NoteOn>(events[0].payload).note_id == 1);

    // Event 1 & 2 both happen at sample 24000.
    // Invariant: NoteOff(1) MUST strictly precede NoteOn(2)!
    assert(events[1].sample_position == 24000);
    assert(events[2].sample_position == 24000);

    assert(std::holds_alternative<events::NoteOff>(events[1].payload));
    assert(std::get<events::NoteOff>(events[1].payload).note_id == 1);

    assert(std::holds_alternative<events::NoteOn>(events[2].payload));
    assert(std::get<events::NoteOn>(events[2].payload).note_id == 2);

    // Event 3: NoteOff B at sample 48000
    assert(events[3].sample_position == 48000);
    assert(std::holds_alternative<events::NoteOff>(events[3].payload));
    assert(std::get<events::NoteOff>(events[3].payload).note_id == 2);

    std::cout << "[PASS] test_legato_retrigger_boundary_ordering\n";
}

void test_multilane_and_polyphony() {
    std::cout << "[RUN] test_multilane_and_polyphony\n";

    time::TempoMap tempo(120.0);
    model::Pattern pattern(1, "ChordPattern", time::BeatDuration::from_beats(4));

    const auto lane_lead = pattern.add_lane("Lead");
    const auto lane_pad = pattern.add_lane("Pad");

    auto* lead = pattern.find_lane(lane_lead);
    auto* pad = pattern.find_lane(lane_pad);
    assert(lead && pad);

    // Lead melody note at beat 0.0
    lead->notes().add_note(time::BeatPosition::zero(), time::BeatDuration::from_beats(1), 72.0);

    // Pad triad chord at beat 0.0: C4 (60), E4 (64), G4 (67)
    pad->notes().add_note(time::BeatPosition::zero(), time::BeatDuration::from_beats(2), 60.0);
    pad->notes().add_note(time::BeatPosition::zero(), time::BeatDuration::from_beats(2), 64.0);
    pad->notes().add_note(time::BeatPosition::zero(), time::BeatDuration::from_beats(2), 67.0);

    const auto events = model::PatternCompiler::compile(pattern, tempo, 48000.0);
    assert(events.size() == 8); // 4 notes * 2 events each

    // All 4 NoteOns should be at sample 0
    for (size_t i = 0; i < 4; ++i) {
        assert(events[i].sample_position == 0);
        assert(std::holds_alternative<events::NoteOn>(events[i].payload));
    }

    // Lead NoteOff at beat 1.0 -> sample 24000
    assert(events[4].sample_position == 24000);
    assert(std::holds_alternative<events::NoteOff>(events[4].payload));
    assert(std::get<events::NoteOff>(events[4].payload).note_id == 1);

    // Pad 3 NoteOffs at beat 2.0 -> sample 48000
    for (size_t i = 5; i < 8; ++i) {
        assert(events[i].sample_position == 48000);
        assert(std::holds_alternative<events::NoteOff>(events[i].payload));
    }

    std::cout << "[PASS] test_multilane_and_polyphony\n";
}

void test_determinism() {
    std::cout << "[RUN] test_determinism\n";

    time::TempoMap tempo(120.0);
    model::Pattern pattern(1, "ComplexPattern", time::BeatDuration::from_beats(16));

    const auto l1 = pattern.add_lane("L1");
    const auto l2 = pattern.add_lane("L2");

    auto* lane1 = pattern.find_lane(l1);
    auto* lane2 = pattern.find_lane(l2);

    for (int i = 0; i < 8; ++i) {
        lane1->notes().add_note(time::BeatPosition::from_fraction(i, 2),
                               time::BeatDuration::from_fraction(1, 2),
                               60.0 + i);
        lane2->notes().add_note(time::BeatPosition::from_fraction(i, 2),
                               time::BeatDuration::from_fraction(1, 4),
                               48.0 + i);
    }

    const auto baseline = model::PatternCompiler::compile(pattern, tempo, 48000.0);
    assert(!baseline.empty());

    // Compile 100 times and verify strict equality
    for (int iter = 0; iter < 100; ++iter) {
        const auto run = model::PatternCompiler::compile(pattern, tempo, 48000.0);
        assert(run.size() == baseline.size());
        for (size_t i = 0; i < baseline.size(); ++i) {
            assert(run[i].sample_position == baseline[i].sample_position);
            assert(run[i].payload == baseline[i].payload);
        }
    }

    std::cout << "[PASS] test_determinism\n";
}

int main() {
    test_compiler_accuracy_48k_and_44k();
    test_legato_retrigger_boundary_ordering();
    test_multilane_and_polyphony();
    test_determinism();
    std::cout << "ALL PATTERN COMPILER TESTS PASSED!\n";
    return 0;
}
