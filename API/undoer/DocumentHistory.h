#pragma once
/// @file
/// One document's part in undo, whatever its format. A mutation is bracketed by an Edit (RAII); when the outermost closes, the
/// document reports what changed (CollectChanges) and that becomes its part of one labelled step. Commands therefore need no
/// inverse: any code that changes the document inside an Edit is undoable.
///
/// The steps live on a Stack, private to the document until it joins a shared one (Join), so one undo steps across every open
/// document. This class keeps what every format shares: the stack, the dirty state (undoing back to the saved state reads clean,
/// in any order across documents), a revision that moves with every change, and the listeners. A format says only how it finds
/// and restores a change: ItemHistory snapshots the items an edit touches, SnapshotHistory the whole document, and an app can
/// diff its own model (Behavior Kit's CB units).

#include <undoer/Stack.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace undoer {

class DocumentHistory : public Participant {
public:
    using Listener = std::function<void()>;

    DocumentHistory();
    ~DocumentHistory() override;
    DocumentHistory(const DocumentHistory&)            = delete;
    DocumentHistory& operator=(const DocumentHistory&) = delete;

    /// @brief Keep the steps on `stack` from now on, with every open document's. Steps already taken here are forgotten.
    void Join(std::shared_ptr<undoer::Stack> stack);
    /// @brief The address (a Data path, a file) the Refs of this document's steps name it by.
    void SetAddress(std::string address) { m_address = std::move(address); }
    [[nodiscard]] const std::string& Address() const { return m_address; }
    /// @brief The stack its steps are on, the shared one once joined.
    [[nodiscard]] undoer::Stack& GetStack() { return *m_stack; }

    /// @brief An open edit. Nested edits fold into the outermost one, which keeps its label.
    class Edit {
    public:
        Edit(DocumentHistory& h, std::string label) : m_edit(h.m_stack->Begin(std::move(label))) { h.Touch(); }

    private:
        undoer::Stack::Edit m_edit;
    };

    /// @brief Open an edit: `auto edit = history.Begin("Set name");`
    [[nodiscard]] Edit Begin(std::string label) { return Edit(*this, std::move(label)); }
    /// @brief This document is being edited in the step open on its stack (one a caller opened on the stack itself). Begin does it.
    void Touch();

    // The stack's: with a shared stack these step across every document.
    [[nodiscard]] bool CanUndo() const { return m_stack->CanUndo(); }
    [[nodiscard]] bool CanRedo() const { return m_stack->CanRedo(); }
    [[nodiscard]] const std::string& UndoLabel() const { return m_stack->UndoLabel(); }
    [[nodiscard]] const std::string& RedoLabel() const { return m_stack->RedoLabel(); }
    bool Undo() { return m_stack->Undo(); }
    bool Redo() { return m_stack->Redo(); }

    /// @brief True when the document differs from the last MarkSaved()/Reset() state.
    [[nodiscard]] bool IsDirty() const { return m_state != m_saved; }
    void               MarkSaved();
    /// @brief Forget this document's steps and treat its current state as saved (after New or Open).
    void               Reset();

    /// @brief Called when this document changed, or its saved state did.
    void OnChanged(Listener l) { m_listeners.push_back(std::move(l)); }

    /// @brief The document's position in the history: a number unique in the process to the state the document is in now.
    ///        It changes with every step, undo, redo and reset (never with MarkSaved), so a copy derived from the document
    ///        (its serialized form, its GPU meshes) is current while the number it was made at still matches.
    [[nodiscard]] std::uint64_t Revision() const { return m_revision; }

    [[nodiscard]] std::unique_ptr<StepPart> Collect() final;

protected:
    /// @brief What one step changed in this document. Undo and Redo put the document back (Restore); the history then moves its
    ///        dirty state and revision and tells its listeners.
    class Part : public StepPart {
    public:
        explicit Part(DocumentHistory& owner) : m_owner(owner) {}
        void Undo() final;
        void Redo() final;

    protected:
        /// @brief Write the state before the step (`undo`) or after it into the document.
        virtual void Restore(bool undo) = 0;

    private:
        friend class DocumentHistory;
        DocumentHistory& m_owner;
        std::uint64_t    m_before = 0, m_after = 0;
    };

    /// @brief The document joins the open step (its first touch in it), before anything in the step changed it.
    virtual void Joining() {}
    /// @brief What changed since the last Collect; null when nothing did. Called as the outermost edit touching it closes.
    [[nodiscard]] virtual std::unique_ptr<Part> CollectChanges() = 0;
    /// @brief Reset: take the document as it is now as the state the next change is measured from.
    virtual void Recapture() = 0;

private:
    void Notify();
    /// @brief The document moved: a fresh process-unique revision, and `state` is its state now.
    void Moved(std::uint64_t state);
    [[nodiscard]] std::uint64_t NextState() { return ++m_lastState; }

    std::string                    m_address;
    std::shared_ptr<undoer::Stack> m_stack;
    std::uint64_t                  m_revision = 0;
    std::uint64_t                  m_state = 0, m_saved = 0, m_lastState = 0;
    std::vector<Listener>          m_listeners;
};

} // namespace undoer
