# Proud Beta Implementation Ledger

Updated: 2026-09-19

This is the working continuation ledger for the first end-to-end Saudade beta.
It records verified state and remaining implementation order; it is not a
substitute for implementation.

## Recovery findings

- The inherited M7 tree built, but both the GUI and acceptance executable
  aborted during shutdown when PipeWire initialization failed.
- Root cause: `PipeWireEndpoint::loop_thread_fn()` set `running_` false on an
  initialization failure, then `stop()` returned without joining the still
  joinable `std::thread`. Its destructor therefore called `std::terminate`.
- `PipeWireEndpoint::stop()` now always joins a joinable worker.
- GCC debug build and all 16 inherited tests pass after the repair.
- In this managed shell, the inherited `DISPLAY`/`WAYLAND_DISPLAY` sockets are
  inaccessible. Headless GUI verification therefore uses
  `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software`; normal desktop
  PipeWire/Wayland acceptance still remains mandatory before completion.

## Implemented in current recovery phase

- Preserved the existing Stitch UI and M7 note editor/transport/synth work.
- Added canonical `Project`, `Track`, `MixerState`, and `ClipInstance` model
  types with stable IDs and referential validation.
- Migrated the controller's initial pattern into a real project containing an
  instrument track, canonical pattern, lane, and clip.
- Arrangement compilation now walks canonical clips instead of compiling an
  isolated GUI-owned pattern.
- Added model tests for clip identity, shared pattern references, mixer state,
  project extent, and deletion cascades.

## Required execution order

1. Finish canonical arrangement controller operations and replace fake
   Arrangement QML with project-driven data and direct clip interaction.
2. Add a versioned persistence adapter and wire New/Open/Save/Save As, dirty
   state, unsaved confirmation, recent project, and autosave recovery.
3. Add deterministic Standard MIDI File import into canonical patterns.
4. Add one-engine offline render and stereo PCM WAV writer with UI progress.
5. Extend graph/render state for real gain, pan, mute, solo, master gain, and
   bounded peak telemetry; remove all playback-driven fake meters.
6. Finish native synth patch controls and persist them.
7. Complete Piano Roll interaction gaps (start resize, cursor-centered zoom,
   temporary eraser tooltip/shortcut audit, focus-safe global shortcuts).
8. Add behavior tests for persistence, MIDI, WAV, arrangement, mixer,
   telemetry bounds, and real Qt mouse/keyboard events.
9. Run GCC, Clang, ASan+UBSan, and TSan without the inherited leak-sanitizer
   suppression; investigate every failure.
10. Run the full real desktop/PipeWire acceptance scenario and 1000-note
    interaction stress profile, fixing failures before claiming beta status.

