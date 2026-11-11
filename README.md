# UNDOER

One undo core for every document type. C++23, standard library only.

```cmake
FetchContent_Declare(UNDOER SOURCE_DIR D:/Projects/Common/internal/UNDOER)   # or a git repository + tag
FetchContent_MakeAvailable(UNDOER)
target_link_libraries(MyTool PRIVATE undoer::undoer)
```

| Type | What |
|---|---|
| `Stack` | the shared stack of named steps; one step can span documents of any format. Undo, Redo, TakeBack, labels, what the last step touched |
| `DocumentHistory` | one document's part: the RAII edit (`Begin`), joining a shared stack, dirty state, revision, listeners. A format subclasses it |
| `ItemHistory<Item>` | a document that is a list of value items: an edit touches the items it changes; a step keeps only those that differ |
| `SnapshotHistory<Doc>` | whole-document value snapshots through capture / restore callbacks, with a drift guard for unbracketed changes |
| `Clipboard` | a typed single-slot clipboard: Paste<T> is offered only while it holds a T |

```cpp
#include <undoer/Undoer.h>

auto stack = std::make_shared<undoer::Stack>();

std::vector<Block> blocks = Load(path);
undoer::ItemHistory<Block> history(blocks, "nif");
history.SetAddress(path);
history.Join(stack);

{
    auto edit = history.Begin("Move");   // nested edits fold into this one: a drag is one step
    history.Touch(3);
    blocks[3].translation += delta;
}
stack->Undo();                            // blocks[3] is back; history.IsDirty() is false again
```

## Build and test

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug
```
