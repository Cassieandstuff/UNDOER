#pragma once
/// @file
/// The one undo stack of an app or project: named steps, each holding what it changed in every document it touched, so one step
/// can span documents of different formats. It knows no format: a document is a Participant that reports its own change when the
/// step closes.

#include <undoer/Ref.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace undoer {

/// @brief What one step changed in one document.
class StepPart {
public:
    virtual ~StepPart() = default;
    virtual void Undo() = 0;
    virtual void Redo() = 0;
    /// @brief The objects it changed.
    [[nodiscard]] virtual std::vector<Ref> Touched() const = 0;
};

/// @brief A document on a stack.
class Participant {
public:
    virtual ~Participant() = default;
    /// @brief What changed since the last Collect; null when nothing did. Called once per step the document joined, as it closes.
    virtual std::unique_ptr<StepPart> Collect() = 0;
};

class Stack {
public:
    static constexpr std::size_t kDefaultMaxSteps = 256;

    Stack() = default;
    Stack(const Stack&)            = delete;
    Stack& operator=(const Stack&) = delete;

    /// @brief An open step (RAII). Nested edits fold into the outermost, which keeps its label: a drag is one step.
    class Edit {
    public:
        Edit(Stack& s, std::string label) : m_stack(s) { m_stack.BeginEdit(std::move(label)); }
        ~Edit() { m_stack.EndEdit(); }
        Edit(const Edit&)            = delete;
        Edit& operator=(const Edit&) = delete;

    private:
        Stack& m_stack;
    };

    /// @brief Open a step. Every document touched before it closes joins it.
    [[nodiscard]] Edit Begin(std::string label) { return Edit(*this, std::move(label)); }
    void BeginEdit(std::string label);
    void EndEdit();
    /// @brief `p` is being edited in the open step.
    /// @return true when `p` joined the step with this call (the first touch); false when it already had, or no step is open
    bool Touch(Participant& p);
    /// @brief True while a step is open.
    [[nodiscard]] bool Open() const { return m_depth > 0; }

    [[nodiscard]] bool CanUndo() const { return !m_undo.empty(); }
    [[nodiscard]] bool CanRedo() const { return !m_redo.empty(); }
    [[nodiscard]] const std::string& UndoLabel() const;
    [[nodiscard]] const std::string& RedoLabel() const;
    /// @return false with no step to take, or inside an open edit
    bool Undo();
    bool Redo();
    /// @brief Undo the latest step for good: an edit that failed partway (or a cancelled drag) never happened, so it can't be redone.
    /// @return false with no step to take, or inside an open edit
    bool TakeBack();

    /// @brief Forget every step touching `p`, which is going away or starting over.
    void Forget(const Participant& p);

    /// @brief The labels of the steps done, oldest first, then of those undone, nearest first.
    [[nodiscard]] std::vector<std::string> UndoLabels() const;
    [[nodiscard]] std::vector<std::string> RedoLabels() const;
    /// @brief What the latest step, undo or redo changed.
    [[nodiscard]] const std::vector<Ref>& LastTouched() const { return m_lastTouched; }

    /// @brief How many steps it keeps; the oldest go first. Lowering it drops the oldest now.
    void SetMaxSteps(std::size_t n);
    [[nodiscard]] std::size_t MaxSteps() const { return m_maxSteps; }

    /// @brief Called after every step, undo, redo and forget.
    void OnChanged(std::function<void()> l) { m_listeners.push_back(std::move(l)); }
    /// @brief Nobody listens from here on: the listeners' owner is going, and the documents it holds still Forget themselves on the way out.
    void ClearListeners() { m_listeners.clear(); }

private:
    struct Part {
        Participant*              owner = nullptr;
        std::unique_ptr<StepPart> part;
    };
    struct Step {
        std::string       label;
        std::vector<Part> parts;
    };

    void Notify();
    void NoteTouched(const Step& s);

    int                                m_depth = 0;
    std::string                        m_label;
    std::vector<Participant*>          m_open;   ///< the documents of the open step, first touched first
    std::vector<Step>                  m_undo, m_redo;
    std::vector<Ref>                   m_lastTouched;
    std::vector<std::function<void()>> m_listeners;
    std::size_t                        m_maxSteps = kDefaultMaxSteps;
};

} // namespace undoer
