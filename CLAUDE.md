# UNDOER — working notes

One undo core for every document type an editor holds (Behavior Kit's behavior units, NIFs, scenes, materials). Consumers link
`undoer::undoer` and include `<undoer/Undoer.h>`. Built from Behavior Kit's `object::ProjectHistory` (the multi-document stack)
and sct-state's `History<Doc>` / `ScopedEdit` / `Clipboard` (the snapshot discipline).

## Rules
- Standard library only: no Qt, no CB, no format. A format plugs in as a `DocumentHistory` subclass.
- Undo is recorded, never written: no command writes its own inverse. A change made outside an edit is not undoable, and the
  snapshot drift guard says so.
- One step can span documents: every document of an app joins one shared `Stack` (`DocumentHistory::Join`).
- Dirty state is by state id, not by diff: undoing back to the saved state reads clean, in any order across documents.
- Never set `CMAKE_MSVC_RUNTIME_LIBRARY`: it inherits the consumer's.
- Comments are Doxygen annotations only (`///`), carrying the why, contract or gotcha, never a restatement.

## Build and test
- `cmake -S . -B build`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug`.
- `Directory.Build.props` caps the compiler at half the cores; `BUILD_CORES` overrides it. Never pass `-j` or a bare `/MP`.
- `tests/undoer_test.cpp` is a plain harness (no test framework dependency).
