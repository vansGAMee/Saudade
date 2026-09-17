# Saudade Architecture

This document describes the architectural topology, thread boundaries, component relationships, and ownership rules of Saudade.

---

## 1. Component Topology & Dependency Direction

Saudade is organized into layered libraries and standalone applications with strict one-way dependency boundaries:

```
apps/saudade (Qt 6 GUI + EditorController)
    │
    ├──> libs/model (Musical domain: Pattern, NoteSequence, Note)
    │        │
    │        └──> libs/events (TimelineEvent, NoteOn, NoteOff)
    │                 │
    │                 └──> libs/time (BeatPosition, BeatDuration, TempoMap, Transport)
    │
    ├──> libs/audio (AudioEngine, PlanPublisher, ProcessContext, SpscRingBuffer)
    │        │
    │        └──> libs/renderplan (RenderPlan, ExecutionStep, DspState)
    │                 │
    │                 └──> libs/graph (GraphModel, GraphCompiler)
    │
    └──> adapters/pipewire (PipeWireEndpoint via pw_filter)
```

### Invariants:
- **Core Isolation**: `libs/time`, `libs/events`, `libs/model`, `libs/renderplan`, `libs/graph`, and `libs/audio` have **zero** dependencies on Qt, PipeWire, ALSA, or JACK.
- **UI Boundary**: Qt is strictly confined to `apps/saudade` (`PianoRollItem`, `PianoKeysItem`, `EditorController`). Core libraries never include Qt headers or link against Qt libraries.
- **Audio Adapter Boundary**: PipeWire is strictly confined to `adapters/pipewire`. Audio endpoints implement the abstract `IAudioEndpoint` interface declared in `libs/audio`.

---

## 2. Execution Domains: Control Thread vs. Realtime Thread

Saudade enforces a clean physical separation between non-realtime management (Control Thread) and deterministic processing (Realtime Audio Thread):

| Concern | Control Thread (UI / Engine Host) | Realtime Thread (PipeWire / Endpoint) |
|---|---|---|
| **Primary Task** | User input, QML scene graph, compilation, state synchronization | Audio block synthesis, DSP execution, event dispatch |
| **Allocations** | Allowed (standard heap allocation) | **Forbidden** (`ScopedRealtimeGuard` monitored) |
| **Locks & Blocking** | Mutexes, file I/O, condition variables permitted | **Forbidden** (lock-free atomics and SPSC queues only) |
| **Graph Mutations** | Mutable `GraphModel` & compilation to immutable `RenderPlan` | Read-only execution of immutable `RenderPlan` snapshot |
| **Musical Model** | Mutable `Pattern`, `NoteSequence`, editing operations | Read-only pre-compiled `TimelineEvent` stream |
| **Destruction** | Owns and destroys retired plans/buffers (`collect_retired()`) | Never destroys resources or releases heap memory |

---

## 3. Graph Pipeline: GraphModel → GraphCompiler → RenderPlan

Audio synthesis and DSP processing are modeled as a Directed Acyclic Graph (DAG) that compiles into a flat, branchless execution list:

1. **GraphModel (`libs/graph`)**:
   - Represents audio nodes (`SineNode`, `PolySynthNode`, `GainNode`, `OutputNode`) and directed audio/event connections.
   - Mutable, validated for cycles and port type compatibility.

2. **GraphCompiler (`libs/graph`)**:
   - Performs topological sorting via Kahn's algorithm.
   - Allocates scratch audio channels and buffers.
   - Compiles the graph into an immutable `RenderPlan`.

3. **RenderPlan (`libs/renderplan`)**:
   - Flat sequence of pre-allocated `ExecutionStep` structures.
   - Immutable once compiled: contains no pointers back to `GraphModel`.
   - Accompanied by pre-allocated `DspStatePool` managing persistent state across quantum swaps.

4. **Plan Publication Protocol (`PlanPublisher`)**:
   - Non-blocking RCU-style double-buffered publication with atomic generation counters.
   - Realtime thread acquires the current active plan pointer at the start of each quantum.
   - Control thread collects retired plans only after the RT thread acknowledges the transition.

---

## 4. Musical Pipeline: Pattern → PatternCompiler → TimelineEvent → AudioEngine

Musical composition flows from user input down to audio rendering without dynamic allocations in the audio thread:

1. **Musical Model (`libs/model`)**:
   - `Pattern` contains `PatternLane` tracks holding `NoteSequence` containers.
   - `Note` defined by `NoteId`, `BeatPosition`, `BeatDuration`, `pitch`, and `velocity`.
   - Fractional beat positions and durations are stored with 960 PPQN integer tick precision.

2. **PatternCompiler (`libs/model`)**:
   - Converts musical notes into linear sample positions using the `TempoMap`.
   - Emits canonical `NoteOn` and `NoteOff` events sorted deterministically:
     1. Sample position ascending;
     2. `NoteOff` before `NoteOn` at identical samples (preventing voice starvation);
     3. Unique `NoteId` ascending.

3. **Event Ingress (`libs/events`, `libs/audio`)**:
   - Compiled `TimelineEvent`s are pushed into an allocation-free single-producer single-consumer ring buffer (`SpscEventQueue`).
   - The RT thread consumes events at quantum boundaries and slices DSP processing to sample-accurate offsets.

---

## 5. UI Boundary & Qt Quick Scene Graph

The user interface (`apps/saudade`) is built using Qt 6 and Qt Quick:

- **EditorController (`apps/saudade/src/editor_controller.cpp`)**:
  - Central mediator connecting `Pattern`, `AudioEngine`, and the UI.
  - Implements CRUD operations on notes (`addNote`, `moveNote`, `resizeNote`, `removeNote`).
  - Prepares playback by compiling notes and enqueueing them to `AudioEngine`.
  - Exposes properties and signals (`notesChanged`, `currentBeatChanged`, `isPlaying`) to QML.

- **Scene Graph Items (`PianoRollItem`, `PianoKeysItem`)**:
  - Subclasses of `QQuickItem` utilizing native `QSGGeometryNode` and `QSGVertexColorMaterial`.
  - Notes, background grid, and playhead are rendered as custom vertex geometry batches.
  - Eliminates QML delegate allocation and layout overhead, maintaining 60+ FPS during editing and playback.

---

## 6. Ownership and Lifetime Rules

- **RenderPlan Ownership**:
  - `AudioEngine` retains single ownership of the active plan and retired plans via `std::unique_ptr`.
  - The RT thread references the active plan exclusively via non-owning raw pointer (`const RenderPlan*`), guaranteed alive throughout the quantum duration.

- **Memory Reclamation**:
  - The RT thread **never** invokes destructors or releases heap memory.
  - Control thread periodically calls `AudioEngine::collect_retired()` to reclaim retired plans and flushed event blocks once their generation has been acknowledged by the RT thread.
