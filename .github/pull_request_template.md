## What changed
Brief description of the changes introduced in this PR.

## Why
Motivation, rationale, or issue reference.

## Architecture impact
- [ ] No changes to architectural boundaries or dependencies
- [ ] Dependencies or boundaries modified (describe details below):

## Realtime impact
- [ ] Confirmed zero allocations, locks, blocking calls, or logging in the realtime audio path
- [ ] Verified via ScopedRealtimeGuard / allocation guard tests

## Tests
- [ ] GCC Debug passes (ctest --test-dir build)
- [ ] Clang Debug passes (ctest --test-dir build-clang)
- [ ] ASan + UBSan passes (ctest --test-dir build-asan)
- [ ] TSan passes (ctest --test-dir build-tsan)
- [ ] Deterministic unit tests added/updated
