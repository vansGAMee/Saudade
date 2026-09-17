# Saudade - Professional Linux DAW in C++23

Saudade is a Linux-first digital audio workstation (DAW) designed for professional, long-term development in C++23.

---

## Architectural Pipeline

```text
┌─────────────────────────────────────────────────────────────┐
│                    CONTROL SIDE (UI / Model)                │
│                                                             │
│   Pattern                                                   │
│     └─ PatternLane                                          │
│          └─ NoteSequence                                    │
│               └─ Note (BeatPosition, BeatDuration, pitch)   │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                    PatternCompiler                          │
│                                                             │
│   TempoMap::beat_to_sample(beat, sample_rate)               │
│   Deterministic sorting:                                    │
│     1. sample_position ASC                                  │
│     2. NoteOff BEFORE NoteOn at identical sample            │
│     3. NoteId ASC                                           │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               │ std::vector<TimelineEvent>
                               ▼
┌─────────────────────────────────────────────────────────────┐
│               AudioEngine::schedule_event()                 │
│               (SpscEventQueue ring buffer)                  │
└──────────────────────────────┬──────────────────────────────┘
                               │
═══════════════════════════════╪═══════════════════════════════
                      REALTIME THREAD BOUNDARY
═══════════════════════════════╪═══════════════════════════════
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                       AudioEngine                           │
│   - ProcessContext (sample_rate, num_frames, start_sample)  │
│   - Quantum Event Ingress & Relative Sample Slicing         │
│   - 0 allocations, 0 locks, 0 logging                       │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                       RenderPlan                            │
│   - PolySynthStep (8 voices, oldest-voice stealing)         │
│   - GainStep (-12 dB)                                       │
│   - OutputStep (Stereo)                                     │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                   PipeWireEndpoint (pw_filter)              │
│   - Real hardware audio playback                            │
└─────────────────────────────────────────────────────────────┘
```

---

## Architectural Principles & Invariants

1. **Isolation of Audio Core**:
   - `libs/time`, `libs/events`, `libs/model`, `libs/renderplan`, `libs/graph`, and `libs/audio` have zero dependencies on Qt, PipeWire, ALSA, or JACK.
   - Qt is strictly confined to the UI layer (`apps/saudade`).
   - PipeWire is strictly confined to `adapters/pipewire`.

2. **Hard Realtime Safety**:
   The realtime render callback strictly guarantees:
   - Zero dynamic allocations (`new`, `delete`, `malloc`, `free`, vector reallocations).
   - Zero blocking locks (`mutex`, `condition_variable`, lock contention).
   - Zero I/O operations (file system, console output, `printf`, `iostream`).
   - Monitored by a thread-local `ScopedRealtimeGuard`.

3. **RT-Safe Event Flush Handshake**:
   - Seamless repeated playback cycles (`Draw -> Play -> Stop -> Edit -> Play`).
   - SPSC queue flushed exclusively by the realtime consumer thread at quantum boundaries via atomic generation handshake without mutexes or sleeping.

4. **Scene Graph Piano Roll**:
   - Custom `QQuickItem` with native Qt Quick Scene Graph (`QSGGeometryNode`).
   - Direct batch rendering with zero QML delegate overhead.

---

## Dependencies

- **Compiler**: Clang (>= 18) or GCC (>= 13) supporting C++23
- **Build System**: CMake (>= 3.25) and Ninja
- **Audio**: `libpipewire-0.3` and `pkg-config`
- **GUI**: Qt 6 (Core, Gui, Quick, Qml)

### Package Installation

```bash
# Arch Linux
sudo pacman -S base-devel cmake ninja pipewire clang gcc pkgconf qt6-base qt6-declarative

# Ubuntu 24.04+ / Debian 13+
sudo apt install build-essential cmake ninja-build libpipewire-0.3-dev pkg-config clang \
    qt6-base-dev qt6-declarative-dev libgl1-mesa-dev

# Fedora 40+
sudo dnf install gcc-c++ clang cmake ninja-build pipewire-devel pkgconf-pkg-config \
    qt6-qtbase-devel qt6-qtdeclarative-devel mesa-libGL-devel
```

---

## Build Commands

### Standard Build (GCC Debug)
```bash
cmake -B build -G Ninja
cmake --build build
```

### AddressSanitizer + UndefinedBehaviorSanitizer
```bash
cmake -B build-asan -G Ninja -DSAUDADE_ENABLE_SANITIZERS=ON
cmake --build build-asan
```

### ThreadSanitizer
```bash
cmake -B build-tsan -G Ninja -DSAUDADE_ENABLE_TSAN=ON
cmake --build build-tsan
```

### Clang Build
```bash
cmake -B build-clang -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build-clang
```

---

## Running Tests

Run the complete deterministic test suite (14 test suites):
```bash
ctest --test-dir build --output-on-failure
```

---

## Running the GUI Application

Launch the native Saudade composer application:
```bash
./build/apps/saudade/saudade
```

### Composer Workflow:
1. **Draw Notes**: Left-click and drag on the Piano Roll grid to create notes quantized to 1/4 beats.
2. **Move Notes**: Left-click and drag the body of any note to shift its musical start time or semitone pitch.
3. **Resize Notes**: Left-click and drag the right edge of any note to alter its duration.
4. **Delete Notes**: Right-click on any note to remove it.
5. **Play**: Click **[PLAY]** on the top bar to hear your melody rendered through the 8-voice polyphonic synthesizer via PipeWire.
6. **Stop & Re-edit**: Click **[STOP]**, edit or add notes, and press **[PLAY]** again.

---

## Running the Headless Audio Proof

To run the automated headless audio proof executable:
```bash
./build/apps/audio-proof/audio-proof
```
