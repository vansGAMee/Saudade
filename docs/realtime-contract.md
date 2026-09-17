# Realtime Safety Contract

This document outlines the strict realtime guarantees and invariants governing all code executed within the audio render path of Saudade.

---

## 1. Absolute Realtime Prohibitions

Any function running on the Realtime (RT) audio thread—including `AudioEngine::process()`, `RenderPlan::execute()`, DSP steps, and endpoint callback loops—must strictly adhere to the following prohibitions:

### 1. No Dynamic Memory Allocation or Deallocation
- **Prohibited**: Calls to `malloc()`, `calloc()`, `realloc()`, `free()`, `operator new`, `operator delete`, or standard library containers triggering reallocations (e.g., `std::vector::push_back`, `std::string`).
- **Enforcement**: Monitored at runtime via thread-local `ScopedRealtimeGuard` which aborts on heap allocations.
- **Remedy**: Pre-allocate all audio buffers, DSP states, scratch channels, and event queues during the control-side initialization/compilation phase.

### 2. No Blocking Locks or Synchronization Primitives
- **Prohibited**: `std::mutex`, `std::recursive_mutex`, `std::shared_mutex`, `std::condition_variable`, POSIX mutexes, semaphores, or spinlocks with unbounded wait loops.
- **Permitted**: Bounded lock-free atomic operations (`std::atomic` with `std::memory_order_relaxed`, `acquire`, or `release`) and single-producer single-consumer ring buffers (`SpscEventQueue`).

### 3. No Blocking System Calls or Thread Sleeps
- **Prohibited**: `usleep()`, `nanosleep()`, `std::this_thread::sleep_for()`, `sched_yield()` loops, blocking socket calls, or wait loops dependent on external threads.

### 4. No Filesystem or Network Operations
- **Prohibited**: File opening, reading, writing, seeking (`open()`, `read()`, `write()`, `std::ifstream`), DNS lookups, or network socket interactions. All sample data and presets must be loaded in advance on the control thread.

### 5. No Logging or Console I/O
- **Prohibited**: `std::cout`, `std::cerr`, `printf()`, `snprintf()`, `syslog()`, or unbuffered disk log writers.
- **Remedy**: Lock-free status flags or telemetry ring buffers drained asynchronously by background worker threads.

### 6. No GUI or Qt Framework Access
- **Prohibited**: Calling methods on `QObject`, emitting Qt signals, interacting with the QML engine, touching scene graph nodes, or dispatching events to Qt's event loop from the RT thread.

### 7. No Graph or DSP Topology Compilation
- **Prohibited**: Modifying `GraphModel`, running `GraphCompiler`, compiling `Pattern` instances, or allocating `RenderPlan` structures inside the audio loop.
- **Mechanism**: The control thread compiles new immutable plans and publishes them via the atomic double-buffered `PlanPublisher` generation protocol.

### 8. No Destruction or Deallocation of Ownership
- **Prohibited**: Destructing smart pointers (`std::shared_ptr`, `std::unique_ptr`) or invoking object destructors that could trigger heap deallocations.
- **Rule**: The RT thread reads active plans and event snapshots via non-owning raw pointers/references. Retirement and destruction are deferred to `AudioEngine::collect_retired()` on the control thread.

---

## 2. Realtime Execution Invariants

1. **Deterministic Quantum Budget**:
   Every quantum processing pass must complete in `O(N)` time bounded strictly by the buffer frame size (e.g., 64 to 1024 frames) without worst-case spikes.

2. **Single Snapshot Consistency**:
   The active `RenderPlan` snapshot and `TransportSnapshot` are acquired exactly once at the beginning of the quantum and remain pinned for the entire duration of the block.

3. **Sample-Accurate Event Processing**:
   Events scheduled for the current quantum are ingested from the lock-free queue, sorted by relative frame index, and dispatched to DSP units at their exact intra-block offsets.

---

## 3. Verification & Enforcement

All realtime guarantees are continually tested and verified across CI and developer test runs:

- **Allocation Guard Test**: `tests/test_allocation_guard.cpp` validates that `ScopedRealtimeGuard` reliably catches unexpected allocations.
- **Stress Concurrency Tests**: `tests/test_stress_concurrency.cpp` and `tests/test_quantum_consistency.cpp` ensure zero data races or deadlocks under continuous concurrent plan swapping.
- **Automated CI Sanitizers**: Builds run under AddressSanitizer, UndefinedBehaviorSanitizer, and ThreadSanitizer to guarantee lock-free memory safety.
