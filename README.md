# Saudade - Architecture Proof v0

Saudade is a Linux-first digital audio workstation (DAW) designed for professional, long-term development in C++23.

This repository implements the **Architecture Proof v0** milestone: an end-to-end vertical slice proving the core audio execution pipeline:

```text
GraphModel
    ↓
GraphCompiler
    ↓
RenderPlan
    ↓
AudioEngine
    ├── OfflineEndpoint  → deterministic memory tests
    └── PipeWireEndpoint → real hardware audio output
```

The executable audio graph is:
```text
SineOscillator (440 Hz)
        ↓
Gain (-12 dB)
        ↓
Output (Stereo: Left & Right)
```

---

## Architectural Principles & Invariants

1. **Isolation of Audio Core**:
   - `libs/audio`, `libs/renderplan`, and `libs/graph` have zero dependencies on PipeWire, ALSA, JACK, or Linux-specific audio APIs.
   - All PipeWire interaction is strictly confined within `adapters/pipewire`.

2. **Editable Model vs Executable Plan**:
   - `GraphModel` is an editable graph representation used only on the control/main thread.
   - The audio callback never traverses `GraphModel`.
   - Before playback, `GraphCompiler` validates the graph and compiles it into an immutable `RenderPlan`.

3. **Structurally Immutable `RenderPlan`**:
   - Topology, routing, buffer allocation slots, and execution steps cannot be modified during playback.
   - Mutable DSP runtime state (e.g. oscillator phase) is preallocated outside the realtime thread.

4. **Hard Realtime Safety**:
   The realtime render callback strictly prohibits:
   - Dynamic allocations (`new`, `delete`, `malloc`, `free`, `vector` growth).
   - Blocking synchronization (`mutex`, `condition_variable`, lock contention).
   - I/O operations (file system, network, `printf`, `iostream`, logging).
   - Graph compilation or topological sorting.
   All buffers and states are pre-allocated before audio processing begins.

5. **Instrumentation & Guardrails**:
   - A thread-local `ScopedRealtimeGuard` intercepts dynamic memory allocations inside the realtime render path to guarantee zero allocations.

---

## Dependencies

- **Compiler**: Clang (>= 18) or GCC (>= 13) supporting C++23
- **Build System**: CMake (>= 3.25) and Ninja
- **Libraries**:
  - `libpipewire-0.3` (for the PipeWire adapter and proof executable)
  - `pkg-config`

On Arch Linux / Fedora / Ubuntu:
```bash
# Arch Linux
sudo pacman -S base-devel cmake ninja pipewire clang gcc pkgconf

# Ubuntu 24.04+
sudo apt install build-essential cmake ninja-build libpipewire-0.3-dev pkg-config clang
```

---

## Build Commands

### Release / Standard Build
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Sanitizer Build (AddressSanitizer + UndefinedBehaviorSanitizer)
```bash
cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSAUDADE_ENABLE_SANITIZERS=ON
cmake --build build-asan
```

---

## Running Tests

Run the automated deterministic test suite using `ctest`:
```bash
ctest --test-dir build --output-on-failure
```

The tests verify:
- **Buffer Length**: Exact frame and channel count produced.
- **Finite Values**: Output is completely free of `NaN` and `Inf`.
- **Frequency Accuracy**: Measured signal frequency corresponds to 440 Hz (via zero-crossings).
- **Gain Precision**: Peak amplitude corresponds to -12 dB linear gain (`~0.2512`).
- **Phase Continuity Across Blocks**: Multi-block rendering preserves oscillator phase continuously with zero boundary clicks.
- **Zero Realtime Allocations**: Monitored by `ScopedRealtimeGuard`.
- **Graph Validation & Rejection**: Correct DAG ordering, rejection of cycles, missing outputs, and invalid connections.

---

## Running `audio-proof`

To test physical audio playback through PipeWire:
```bash
./build/apps/audio-proof/audio-proof
```

### Expected Behavior
1. The application initializes PipeWire and queries actual negotiated audio parameters.
2. The startup banner is displayed:
   ```text
   Audio backend: PipeWire
   Sample rate: <actual, e.g. 48000>
   Quantum: <actual, e.g. 512>
   Graph: Sine(440 Hz) -> Gain(-12 dB) -> Output
   Playing. Press Ctrl+C to stop.
   ```
3. A continuous, clear 440 Hz sine tone at -12 dB plays through your default audio output (speakers or headphones).
4. Pressing `Ctrl+C` terminates playback cleanly and exits with code 0.

---

## Current Scope & Intentional Omissions

This milestone is an architectural foundation, not a complete DAW. The following subsystems are **deliberately NOT implemented** in this milestone:

- GUI / Qt / Wayland interfaces
- Audio file loading / WAV playback
- Transport timeline / sequencer
- MIDI / MIDI 2.0
- Plugin hosting (CLAP, VST3, LV2)
- Multi-track mixer
- Project file storage / SQLite / serialization
- Live graph swapping / runtime recompilation
- Multi-threaded DSP worker pools / task stealing

These features will be built on top of this validated foundation in subsequent milestones.
