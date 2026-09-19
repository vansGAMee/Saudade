# Saudade
<img width="692" height="388" alt="2026-09-19 12-42-41" src="https://github.com/user-attachments/assets/98c7d38d-0341-4237-9f9a-7105053d2d65" />

[first_saudade_track1.wav](https://github.com/user-attachments/files/32412425/first_saudade_track1.wav)


<img width="1122" height="1402" alt="ChatGPT Image Sep 18, 2026, 10_51_42 AM" src="https://github.com/user-attachments/assets/db492afa-c3d5-448f-b9f1-2fe6d3e73d74" />

Saudade is a Linux-first open-source professional Digital Audio Workstation (DAW) written in C++23.

> **Status**: Pre-alpha / active development. The project is establishing its architectural foundation and core realtime execution layer.

---

## Current Capabilities

- **Native Audio Backend**: Direct PipeWire integration via `pw_filter` for pro-audio hardware output.
- **Immutable RenderPlan**: Lock-free, allocation-free execution steps compiled from audio graph topologies.
- **Realtime-Safe Plan Swapping**: Generation-acknowledged double-buffered plan publication with safe deferred resource reclamation.
- **Time Core & Transport**: Sample-accurate position tracking (`SamplePosition`), fixed-point beat timing (`BeatPosition`, `BeatDuration`), tempo mapping, and musical transport state.
- **Sample-Accurate Events**: Deterministic `TimelineEvent` ingress and dispatch (`NoteOn`, `NoteOff`) without allocations during quantum processing.
- **Musical Model**: Canonical `Pattern`, `PatternLane`, and `NoteSequence` hierarchy compiled deterministically into sample-accurate event streams.
- **Polyphonic Synthesizer**: 8-voice proof polyphonic synthesizer with band-limited oscillators, ADSR envelopes, and oldest-voice stealing.
- **Qt 6 / Qt Quick GUI**: Desktop frontend with custom Scene Graph rendering (`QSGGeometryNode`).
- **Interactive Piano Roll**: Note creation, dragging to move (time & pitch), edge-dragging to resize duration, right-click deletion, and playback preview.
- **Multi-Toolchain & Multi-Sanitizer Verification**: Continuous testing under GCC, Clang, AddressSanitizer, UndefinedBehaviorSanitizer, and ThreadSanitizer.

---
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/da7fe18e-976b-4a20-9f59-371cd8dd40f3" />

## Architecture

Saudade strictly isolates non-realtime management (control thread) from deterministic DSP rendering (audio thread):

```text
┌─────────────────────────────────────────────────────────────┐
│                    CONTROL SIDE (UI / Model)                │
│   Pattern                                                   │
│     └─ PatternLane                                          │
│          └─ NoteSequence                                    │
│               └─ Note (BeatPosition, BeatDuration, pitch)   │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                    PatternCompiler                          │
│   Deterministic event ordering:                             │
│     1. sample_position ASC                                  │
│     2. NoteOff BEFORE NoteOn at identical sample            │
│     3. NoteId ASC                                           │
└──────────────────────────────┬──────────────────────────────┘
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
│   - PolySynthStep (8 voices, dynamic voice stealing)        │
│   - GainStep (-12 dB master proof attenuation)              │
│   - OutputStep (Interleaved stereo mapping)                 │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                   PipeWireEndpoint (pw_filter)              │
│   - Real hardware audio playback                            │
└─────────────────────────────────────────────────────────────┘
```

For complete structural diagrams and lifetime rules, see [`docs/architecture.md`](docs/architecture.md).

---

## Realtime Guarantees

Code executing in the realtime render path adheres to strict, automated invariants:
- **Zero Dynamic Memory Allocations**: No calls to `malloc`, `free`, `new`, `delete`, or resizing containers (monitored by `ScopedRealtimeGuard`).
- **Zero Blocking Synchronization**: No `std::mutex`, `std::condition_variable`, or unbounded spinlocks.
- **Zero File/Network I/O**: No reading, writing, or filesystem interaction.
- **Zero Logging**: No unbuffered console or file output.
- **Zero Qt / Framework Interactions**: The audio thread remains completely decoupled from GUI libraries.
- **Deferred Destruction**: Resource disposal occurs exclusively on the control thread via `AudioEngine::collect_retired()`.

See [`docs/realtime-contract.md`](docs/realtime-contract.md) for the full contract.

---

## Dependencies

- **Compiler**: GCC >= 13 or Clang >= 18 with C++23 support
- **Build System**: CMake >= 3.25 and Ninja
- **Audio**: `libpipewire-0.3` and `pkg-config`
- **GUI**: Qt 6 (`Core`, `Gui`, `Quick`, `Qml`, `Test`)

### Package Installation

```bash
# Ubuntu 24.04+ / Debian 13+
sudo apt update && sudo apt install -y \
    build-essential cmake ninja-build pkg-config clang gcc g++ \
    libpipewire-0.3-dev libspa-0.2-dev qt6-base-dev qt6-declarative-dev libgl1-mesa-dev

# Arch Linux
sudo pacman -S base-devel cmake ninja pipewire clang gcc pkgconf qt6-base qt6-declarative

# Fedora 40+
sudo dnf install gcc-c++ clang cmake ninja-build pipewire-devel pkgconf-pkg-config \
    qt6-qtbase-devel qt6-qtdeclarative-devel mesa-libGL-devel
```

---

## Build

```bash
# Standard GCC Debug build
cmake -B build -G Ninja
cmake --build build

# Clang build
cmake -B build-clang -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build-clang

# AddressSanitizer + UndefinedBehaviorSanitizer
cmake -B build-asan -G Ninja -DSAUDADE_ENABLE_SANITIZERS=ON
cmake --build build-asan

# ThreadSanitizer
cmake -B build-tsan -G Ninja -DSAUDADE_ENABLE_TSAN=ON
cmake --build build-tsan
```

---

## Test

Run all 14 deterministic test suites:
```bash
ctest --test-dir build --output-on-failure
```

Under sanitizers:
```bash
ctest --test-dir build-asan --output-on-failure
ctest --test-dir build-tsan --output-on-failure
ctest --test-dir build-clang --output-on-failure
```

---

## Run

### GUI Application
Launch the Saudade composer application:
```bash
./build/apps/saudade/saudade
```

- **Draw**: Left-click on empty grid cells to create notes (quantized to 1/4 beats).
- **Move**: Drag a note's body to shift its beat position and pitch.
- **Resize**: Drag a note's right edge to change duration.
- **Delete**: Right-click on a note to delete it.
- **Play / Stop**: Toggle playback to hear the sequence synthesized in real time via PipeWire.

### Headless Audio Proof
To run the automated headless audio proof executable:
```bash
./build/apps/audio-proof/audio-proof
```

---

## Repository Layout

```text
Saudade/
├── adapters/
│   └── pipewire/          # Native PipeWire audio endpoint adapter
├── apps/
│   ├── audio-proof/       # Headless CLI audio verification proof
│   └── saudade/           # Qt 6 / Qt Quick GUI composer application
├── docs/                  # Architecture and realtime contract documentation
├── libs/
│   ├── audio/             # AudioEngine, PlanPublisher, and buffer management
│   ├── events/            # TimelineEvent model and SPSC lock-free event queues
│   ├── graph/             # Mutable audio graph model and topological compiler
│   ├── model/             # Musical domain (Pattern, NoteSequence, Note)
│   ├── renderplan/        # Immutable DSP execution steps and state pools
│   └── time/              # SamplePosition, BeatPosition, TempoMap, Transport
└── tests/                 # Deterministic unit, integration, and sanitizer tests
```

---

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for coding standards, architectural guidelines, commit conventions, and pull request workflows.

---

## License

Saudade is released under the **GNU General Public License v3.0 or later** ([GPL-3.0-or-later](LICENSE)).
