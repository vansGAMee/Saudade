#include <saudade/model/note.hpp>
#include <saudade/model/note_sequence.hpp>
#include <saudade/model/pattern.hpp>

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using namespace saudade;

void test_note_invariants() {
    std::cout << "[RUN] test_note_invariants\n";

    // Valid note
    model::Note valid{
        .note_id = 1,
        .start = time::BeatPosition::zero(),
        .duration = time::BeatDuration::from_beats(1),
        .pitch = 60.0,
        .velocity = 0.8f,
        .release_velocity = 0.2f
    };
    assert(valid.is_valid());

    // Invalid note_id (0)
    auto bad_id = valid;
    bad_id.note_id = 0;
    assert(!bad_id.is_valid());

    // Invalid start (< 0)
    auto bad_start = valid;
    bad_start.start = time::BeatPosition::from_ticks(-1);
    assert(!bad_start.is_valid());

    // Invalid duration (<= 0)
    auto bad_dur_zero = valid;
    bad_dur_zero.duration = time::BeatDuration::from_ticks(0);
    assert(!bad_dur_zero.is_valid());

    auto bad_dur_neg = valid;
    bad_dur_neg.duration = time::BeatDuration::from_ticks(-100);
    assert(!bad_dur_neg.is_valid());

    // Invalid pitch (NaN / Inf)
    auto bad_pitch_nan = valid;
    bad_pitch_nan.pitch = std::numeric_limits<double>::quiet_NaN();
    assert(!bad_pitch_nan.is_valid());

    auto bad_pitch_inf = valid;
    bad_pitch_inf.pitch = std::numeric_limits<double>::infinity();
    assert(!bad_pitch_inf.is_valid());

    // Invalid velocity (< 0.0 or > 1.0)
    auto bad_vel_low = valid;
    bad_vel_low.velocity = -0.01f;
    assert(!bad_vel_low.is_valid());

    auto bad_vel_high = valid;
    bad_vel_high.velocity = 1.01f;
    assert(!bad_vel_high.is_valid());

    // Invalid release velocity (< 0.0 or > 1.0)
    auto bad_rel_low = valid;
    bad_rel_low.release_velocity = -0.1f;
    assert(!bad_rel_low.is_valid());

    auto bad_rel_high = valid;
    bad_rel_high.release_velocity = 1.05f;
    assert(!bad_rel_high.is_valid());

    std::cout << "[PASS] test_note_invariants\n";
}

void test_note_identity_and_removal() {
    std::cout << "[RUN] test_note_identity_and_removal\n";

    model::NoteSequence seq;
    assert(seq.empty());
    assert(seq.size() == 0);

    // Add notes via convenience overload
    auto id1 = seq.add_note(time::BeatPosition::zero(), time::BeatDuration::from_beats(1), 60.0);
    assert(id1.has_value());
    assert(*id1 == 1);

    auto id2 = seq.add_note(time::BeatPosition::from_beats(1), time::BeatDuration::from_beats(1), 64.0);
    assert(id2.has_value());
    assert(*id2 == 2);

    auto id3 = seq.add_note(time::BeatPosition::from_beats(2), time::BeatDuration::from_beats(1), 67.0);
    assert(id3.has_value());
    assert(*id3 == 3);

    assert(seq.size() == 3);

    // Duplicate NoteId insertion rejection
    model::Note duplicate{
        .note_id = *id2,
        .start = time::BeatPosition::from_beats(5),
        .duration = time::BeatDuration::from_beats(1),
        .pitch = 72.0,
        .velocity = 0.9f,
        .release_velocity = 0.0f
    };
    assert(!seq.add_note(duplicate));
    assert(seq.size() == 3);

    // Removal of non-existent note
    assert(!seq.remove_note(999));
    assert(seq.size() == 3);

    // Removal of note id2
    assert(seq.remove_note(*id2));
    assert(seq.size() == 2);
    assert(seq.find_note(*id2) == nullptr);
    assert(seq.find_note(*id1) != nullptr);
    assert(seq.find_note(*id3) != nullptr);

    // Re-adding a note with fresh ID
    auto id4 = seq.add_note(time::BeatPosition::from_beats(3), time::BeatDuration::from_beats(1), 71.0);
    assert(id4.has_value());
    assert(*id4 == 4);
    assert(seq.size() == 3);

    std::cout << "[PASS] test_note_identity_and_removal\n";
}

void test_sequence_operations_and_invariants() {
    std::cout << "[RUN] test_sequence_operations_and_invariants\n";

    model::NoteSequence seq;
    auto id_opt = seq.add_note(time::BeatPosition::zero(), time::BeatDuration::from_beats(1), 60.0, 0.8f, 0.1f);
    assert(id_opt.has_value());
    const auto id = *id_opt;

    // move_note
    assert(seq.move_note(id, time::BeatPosition::from_beats(2)));
    assert(seq.find_note(id)->start == time::BeatPosition::from_beats(2));

    // move_note rejection on negative start
    assert(!seq.move_note(id, time::BeatPosition::from_ticks(-10)));
    assert(seq.find_note(id)->start == time::BeatPosition::from_beats(2));

    // resize_note
    assert(seq.resize_note(id, time::BeatDuration::from_fraction(1, 2)));
    assert(seq.find_note(id)->duration == time::BeatDuration::from_fraction(1, 2));

    // resize_note rejection on non-positive duration
    assert(!seq.resize_note(id, time::BeatDuration::from_ticks(0)));
    assert(!seq.resize_note(id, time::BeatDuration::from_ticks(-5)));
    assert(seq.find_note(id)->duration == time::BeatDuration::from_fraction(1, 2));

    // update_pitch
    assert(seq.update_pitch(id, 62.5));
    assert(seq.find_note(id)->pitch == 62.5);

    // update_pitch rejection on NaN / Inf
    assert(!seq.update_pitch(id, std::numeric_limits<double>::quiet_NaN()));
    assert(!seq.update_pitch(id, std::numeric_limits<double>::infinity()));
    assert(seq.find_note(id)->pitch == 62.5);

    // update_velocity
    assert(seq.update_velocity(id, 0.95f, 0.4f));
    assert(seq.find_note(id)->velocity == 0.95f);
    assert(seq.find_note(id)->release_velocity == 0.4f);

    // update_velocity rejection on out of bounds
    assert(!seq.update_velocity(id, 1.2f, 0.4f));
    assert(!seq.update_velocity(id, -0.1f, 0.4f));
    assert(!seq.update_velocity(id, 0.5f, 1.1f));
    assert(!seq.update_velocity(id, 0.5f, -0.2f));

    // update_note wholesale
    model::Note replacement{
        .note_id = id,
        .start = time::BeatPosition::from_beats(4),
        .duration = time::BeatDuration::from_beats(2),
        .pitch = 72.0,
        .velocity = 0.6f,
        .release_velocity = 0.3f
    };
    assert(seq.update_note(replacement));
    assert(*seq.find_note(id) == replacement);

    // clear
    seq.clear();
    assert(seq.empty());
    assert(seq.size() == 0);

    std::cout << "[PASS] test_sequence_operations_and_invariants\n";
}

void test_pattern_and_multi_lane() {
    std::cout << "[RUN] test_pattern_and_multi_lane\n";

    model::Pattern pattern(42, "LeadPattern", time::BeatDuration::from_beats(8));
    assert(pattern.id() == 42);
    assert(pattern.name() == "LeadPattern");
    assert(pattern.length() == time::BeatDuration::from_beats(8));
    assert(pattern.num_lanes() == 0);

    // Length validation
    assert(pattern.set_length(time::BeatDuration::from_beats(16)));
    assert(pattern.length() == time::BeatDuration::from_beats(16));
    assert(!pattern.set_length(time::BeatDuration::from_ticks(0)));
    assert(!pattern.set_length(time::BeatDuration::from_ticks(-1)));
    assert(pattern.length() == time::BeatDuration::from_beats(16));

    // Add lanes
    const auto lane1 = pattern.add_lane("SynthLead");
    const auto lane2 = pattern.add_lane("Bass");
    assert(pattern.num_lanes() == 2);
    assert(lane1 != lane2);

    auto* l1 = pattern.find_lane(lane1);
    auto* l2 = pattern.find_lane(lane2);
    assert(l1 != nullptr && l2 != nullptr);
    assert(l1->name() == "SynthLead");
    assert(l2->name() == "Bass");

    // Add notes to lane 1
    auto n1 = l1->notes().add_note(time::BeatPosition::zero(), time::BeatDuration::from_beats(1), 60.0);
    assert(n1.has_value());
    assert(l1->notes().size() == 1);
    assert(l2->notes().size() == 0);

    // Add notes to lane 2
    auto n2 = l2->notes().add_note(time::BeatPosition::zero(), time::BeatDuration::from_beats(2), 36.0);
    assert(n2.has_value());
    assert(l1->notes().size() == 1);
    assert(l2->notes().size() == 1);

    // Remove lane
    assert(pattern.remove_lane(lane1));
    assert(pattern.num_lanes() == 1);
    assert(pattern.find_lane(lane1) == nullptr);
    assert(pattern.find_lane(lane2) != nullptr);

    std::cout << "[PASS] test_pattern_and_multi_lane\n";
}

int main() {
    test_note_invariants();
    test_note_identity_and_removal();
    test_sequence_operations_and_invariants();
    test_pattern_and_multi_lane();
    std::cout << "ALL MODEL TESTS PASSED!\n";
    return 0;
}
