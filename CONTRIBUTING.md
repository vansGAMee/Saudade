# Contributing to Saudade

Thank you for your interest in contributing to Saudade! As a high-performance, Linux-first DAW written in C++23, Saudade maintains strict architectural boundaries, deterministic memory safety, and hard realtime execution guarantees.

---

## 1. Architectural Boundaries & Realtime Rules

Before writing code, please review:
- [`docs/architecture.md`](docs/architecture.md) — Structural layers, dependency topology, and ownership rules.
- [`docs/realtime-contract.md`](docs/realtime-contract.md) — Absolute realtime prohibitions in the audio rendering path.

### Core Rules:
1. **Zero Allocations & Locks in RT**: The audio rendering path (`AudioEngine::process`, DSP execution, audio endpoints) must **never** allocate memory, acquire blocking locks, perform I/O, or interact with Qt.
2. **Layer Isolation**: Core libraries (`libs/*`) must remain free of dependencies on Qt or PipeWire. Qt belongs solely in `apps/saudade`; PipeWire belongs solely in `adapters/pipewire`.
3. **Deterministic Testing**: Every new DSP feature, compiler phase, or data structure must be accompanied by comprehensive, deterministic unit/integration tests.

---

## 2. Environment & Dependencies

- **Compiler**: GCC 13+ or Clang 18+ supporting C++23
- **Build System**: CMake 3.25+ and Ninja
- **Libraries**:
  - `libpipewire-0.3-dev` and `libspa-0.2-dev`
  - `qt6-base-dev` and `qt6-declarative-dev`
  - `pkg-config`

---

## 3. Build & Test Commands

Saudade supports multiple build configurations. All PRs must compile with zero warnings across both GCC and Clang, and pass all tests under ASan/UBSan and TSan.

### Standard Build (GCC Debug)
```bash
cmake -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

### Clang Build
```bash
cmake -B build-clang -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build-clang
ctest --test-dir build-clang --output-on-failure
```

### AddressSanitizer + UndefinedBehaviorSanitizer (ASan/UBSan)
```bash
cmake -B build-asan -G Ninja -DSAUDADE_ENABLE_SANITIZERS=ON
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

### ThreadSanitizer (TSan)
```bash
cmake -B build-tsan -G Ninja -DSAUDADE_ENABLE_TSAN=ON
cmake --build build-tsan
ctest --test-dir build-tsan --output-on-failure
```

---

## 4. Commit Convention

Saudade adheres to the [Conventional Commits](https://www.conventionalcommits.org/) specification:

- `feat(scope):` New feature or functionality
- `fix(scope):` Bug fix
- `refactor(scope):` Code change that neither fixes a bug nor adds a feature
- `test(scope):` Adding or modifying tests
- `docs(scope):` Documentation updates
- `build(scope):` Changes affecting the build system or external dependencies
- `ci(scope):` Changes to CI configuration or scripts

### Common scopes:
`audio`, `renderplan`, `graph`, `time`, `events`, `model`, `ui`, `pipewire`

---

## 5. Pull Request Guidelines

1. **Verify All Builds**: Ensure `build`, `build-clang`, `build-asan`, and `build-tsan` pass with **0 warnings** and **100% test pass rate**.
2. **Follow PR Template**: Fill out all sections in the pull request template, explicitly noting any architectural or realtime implications.
3. **No Force-Pushes on Main**: The `main` branch is protected; do not rewrite published history.
