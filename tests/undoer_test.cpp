#include <undoer/Undoer.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void Check(bool ok, const char* what, int line) {
    if (ok) return;
    std::printf("FAIL (line %d): %s\n", line, what);
    ++g_failures;
}
#define CHECK(x) Check(static_cast<bool>(x), #x, __LINE__)

using Ints = std::vector<int>;

struct Doc {
    Ints                          items{ 1, 2, 3 };
    undoer::ItemHistory<int>      history{ items, "test" };
};

/// @brief An item with no operator==: every touch counts as a change.
struct Opaque {
    std::string text;
};

struct Settings {
    std::string name = "a";
    int         size = 1;
    bool operator==(const Settings&) const = default;
};

void ItemStep() {
    Doc d;
    d.history.SetAddress("doc");
    const auto start = d.history.Revision();
    {
        auto edit = d.history.Begin("Set second");
        d.history.Touch(1);
        d.items[1] = 20;
    }
    CHECK(d.history.CanUndo());
    CHECK(d.history.UndoLabel() == "Set second");
    CHECK(d.history.IsDirty());
    CHECK(d.history.Revision() != start);
    CHECK(d.history.GetStack().LastTouched().size() == 1);
    CHECK(d.history.GetStack().LastTouched()[0] == (undoer::Ref{ "test", "doc#1", {} }));

    CHECK(d.history.Undo());
    CHECK(d.items == (Ints{ 1, 2, 3 }));
    CHECK(!d.history.IsDirty());
    CHECK(d.history.Redo());
    CHECK(d.items == (Ints{ 1, 20, 3 }));
    CHECK(d.history.IsDirty());
}

void NothingRecorded() {
    Doc d;
    {
        auto edit = d.history.Begin("Touch only");
        d.history.Touch(0);
        d.history.Touch(0);
    }
    CHECK(!d.history.CanUndo());
    CHECK(!d.history.IsDirty());

    d.history.Touch(2);   // no open edit
    d.items[2] = 30;
    {
        auto edit = d.history.Begin("Unrelated");
    }
    CHECK(!d.history.CanUndo());
}

void Nesting() {
    Doc d;
    {
        auto outer = d.history.Begin("Outer");
        d.history.Touch(0);
        d.items[0] = 10;
        d.history.Apply("Inner", [&](Ints& items) {
            d.history.Touch(0);
            items[0] = 11;
            d.history.Touch(2);
            items[2] = 33;
        });
    }
    CHECK(d.history.GetStack().UndoLabels() == std::vector<std::string>{ "Outer" });
    CHECK(d.history.Undo());
    CHECK(d.items == (Ints{ 1, 2, 3 }));
    CHECK(d.history.Redo());
    CHECK(d.items == (Ints{ 11, 2, 33 }));
}

void WholeList() {
    Doc d;
    {
        auto edit = d.history.Begin("Edit then add");
        d.history.Touch(0);
        d.items[0] = 100;
        d.history.TouchAll();
        d.items.push_back(4);
    }
    CHECK(d.history.Undo());
    CHECK(d.items == (Ints{ 1, 2, 3 }));
    CHECK(d.history.Redo());
    CHECK(d.items == (Ints{ 100, 2, 3, 4 }));
}

void NoEquality() {
    std::vector<Opaque>          items{ { "a" } };
    undoer::ItemHistory<Opaque>  history(items, "test");
    {
        auto edit = history.Begin("Set");
        history.Touch(0);
        items[0].text = "b";
    }
    CHECK(history.Undo());
    CHECK(items[0].text == "a");
}

void SavedAndReset() {
    Doc d;
    d.history.Apply("One", [&](Ints& items) {
        d.history.Touch(0);
        items[0] = 5;
    });
    d.history.MarkSaved();
    CHECK(!d.history.IsDirty());
    CHECK(d.history.Undo());
    CHECK(d.history.IsDirty());   // the saved state is the one after the step
    CHECK(d.history.Redo());
    CHECK(!d.history.IsDirty());

    int changes = 0;
    d.history.OnChanged([&] { ++changes; });
    d.history.Reset();
    CHECK(changes == 1);
    CHECK(!d.history.CanUndo());
    CHECK(!d.history.IsDirty());
}

void SharedStack() {
    auto stack = std::make_shared<undoer::Stack>();
    Doc  a, b;
    a.history.Join(stack);
    b.history.Join(stack);
    {
        auto step = stack->Begin("Both");
        a.history.Touch();   // joins the step opened on the stack itself
        a.history.Touch(0);
        a.items[0] = 9;
        b.history.Touch(2);   // Touch(i) joins too
        b.items[2] = 9;
    }
    CHECK(stack->UndoLabels() == std::vector<std::string>{ "Both" });
    CHECK(a.history.IsDirty() && b.history.IsDirty());
    CHECK(stack->Undo());
    CHECK(a.items == (Ints{ 1, 2, 3 }) && b.items == (Ints{ 1, 2, 3 }));
    CHECK(!a.history.IsDirty() && !b.history.IsDirty());

    // A step on one document only, then the other is reset: only the steps touching it go.
    CHECK(stack->Redo());
    a.history.Apply("A alone", [&](Ints& items) {
        a.history.Touch(1);
        items[1] = 8;
    });
    b.history.Reset();
    CHECK(stack->UndoLabels() == std::vector<std::string>{ "A alone" });

    // TakeBack: gone for good.
    CHECK(stack->TakeBack());
    CHECK(!stack->CanRedo());
    CHECK(a.items[1] == 2);
}

void StackRules() {
    undoer::Stack stack;
    stack.SetMaxSteps(2);
    std::vector<int> items{ 0 };
    undoer::ItemHistory<int> h(items, "test");
    auto shared = std::shared_ptr<undoer::Stack>(&stack, [](undoer::Stack*) {});
    h.Join(shared);
    for (int i = 1; i <= 3; ++i)
        h.Apply("Step " + std::to_string(i), [&](std::vector<int>& v) {
            h.Touch(0);
            v[0] = i;
        });
    CHECK(stack.UndoLabels() == (std::vector<std::string>{ "Step 2", "Step 3" }));

    {
        auto open = stack.Begin("Open");
        CHECK(stack.Open());
        CHECK(!stack.Undo());   // never inside an open edit
        CHECK(stack.Touch(h));
        CHECK(!stack.Touch(h));   // already in the step
    }
    CHECK(!stack.Open());
    h.Reset();   // forget its steps before the stack it points at goes
}

void Snapshot() {
    Settings doc;
    auto     history = undoer::MakeValueHistory(doc, "settings");
    history->SetAddress("prefs");
    history->Apply("Rename", [&] { doc.name = "b"; });
    CHECK(history->CanUndo());
    CHECK(history->GetStack().LastTouched()[0] == (undoer::Ref{ "settings", "prefs", {} }));
    history->Apply("Same", [&] { doc.size = 1; });
    CHECK(history->GetStack().UndoLabels().size() == 1);   // no change, no step
    CHECK(history->Undo());
    CHECK(doc.name == "a");
    CHECK(!history->IsDirty());
    CHECK(history->Redo());
    CHECK(doc.name == "b");

    int drift = 0;
    history->onDrift = [&] { ++drift; };
    doc.size = 7;   // outside an edit
    history->Apply("Grow", [&] { doc.size = 8; });
    CHECK(drift == 1);
    CHECK(history->Undo());
    CHECK(doc.size == 7);   // the unbracketed change isn't undoable: undo goes back to where the guard found it

    history->SetDriftGuard(false);
    doc.size = 9;
    history->Apply("Shrink", [&] { doc.size = 1; });
    CHECK(drift == 1);
}

void ClipboardTypes() {
    undoer::Clipboard clip;
    CHECK(clip.Empty());
    clip.Set(std::string("node"));
    CHECK(clip.Has<std::string>());
    CHECK(!clip.Has<int>());
    CHECK(clip.Get<int>() == nullptr);
    CHECK(*clip.Get<std::string>() == "node");
    clip.Clear();
    CHECK(clip.Empty());
}

} // namespace

int main() {
    ItemStep();
    NothingRecorded();
    Nesting();
    WholeList();
    NoEquality();
    SavedAndReset();
    SharedStack();
    StackRules();
    Snapshot();
    ClipboardTypes();
    if (g_failures == 0) std::printf("undoer-test: all passed\n");
    return g_failures == 0 ? 0 : 1;
}
