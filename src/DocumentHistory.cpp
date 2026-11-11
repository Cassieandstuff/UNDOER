#include <undoer/DocumentHistory.h>

#include <atomic>

namespace undoer {

DocumentHistory::DocumentHistory() : m_stack(std::make_shared<undoer::Stack>()) {
    Moved(NextState());
    m_saved = m_state;
}

DocumentHistory::~DocumentHistory() { m_stack->Forget(*this); }

void DocumentHistory::Join(std::shared_ptr<undoer::Stack> stack) {
    m_stack->Forget(*this);
    m_stack = std::move(stack);
}

void DocumentHistory::Touch() {
    if (m_stack->Touch(*this)) Joining();
}

void DocumentHistory::Moved(std::uint64_t state) {
    static std::atomic<std::uint64_t> next{1};
    m_revision = next.fetch_add(1, std::memory_order_relaxed);
    m_state    = state;
}

std::unique_ptr<StepPart> DocumentHistory::Collect() {
    std::unique_ptr<Part> part = CollectChanges();
    if (!part) return nullptr;
    part->m_before = m_state;
    part->m_after  = NextState();
    Moved(part->m_after);
    Notify();
    return part;
}

void DocumentHistory::Part::Undo() {
    Restore(true);
    m_owner.Moved(m_before);
    m_owner.Notify();
}

void DocumentHistory::Part::Redo() {
    Restore(false);
    m_owner.Moved(m_after);
    m_owner.Notify();
}

void DocumentHistory::MarkSaved() {
    m_saved = m_state;
    Notify();
}

void DocumentHistory::Reset() {
    m_stack->Forget(*this);
    Recapture();
    Moved(NextState());
    m_saved = m_state;
    Notify();
}

void DocumentHistory::Notify() {
    for (const auto& l : m_listeners) l();
}

} // namespace undoer
