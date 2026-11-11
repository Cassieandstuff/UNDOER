#include <undoer/Stack.h>

#include <algorithm>
#include <ranges>

namespace undoer {

void Stack::BeginEdit(std::string label) {
    if (m_depth++ == 0) {
        m_label = std::move(label);
        m_open.clear();
    }
}

bool Stack::Touch(Participant& p) {
    if (m_depth <= 0 || std::ranges::find(m_open, &p) != m_open.end()) return false;
    m_open.push_back(&p);
    return true;
}

void Stack::EndEdit() {
    if (m_depth <= 0 || --m_depth != 0) return;
    Step step;
    step.label = std::move(m_label);
    for (Participant* p : m_open)
        if (auto part = p->Collect()) step.parts.push_back({ p, std::move(part) });
    m_open.clear();
    if (step.parts.empty()) return;   // a no-op edit records nothing

    NoteTouched(step);
    m_undo.push_back(std::move(step));
    if (m_undo.size() > m_maxSteps) m_undo.erase(m_undo.begin());
    m_redo.clear();
    Notify();
}

const std::string& Stack::UndoLabel() const {
    static const std::string kNone;
    return m_undo.empty() ? kNone : m_undo.back().label;
}

const std::string& Stack::RedoLabel() const {
    static const std::string kNone;
    return m_redo.empty() ? kNone : m_redo.back().label;
}

bool Stack::Undo() {
    if (m_undo.empty() || m_depth != 0) return false;
    Step step = std::move(m_undo.back());
    m_undo.pop_back();
    for (Part& part : step.parts | std::views::reverse) part.part->Undo();
    NoteTouched(step);
    m_redo.push_back(std::move(step));
    Notify();
    return true;
}

bool Stack::Redo() {
    if (m_redo.empty() || m_depth != 0) return false;
    Step step = std::move(m_redo.back());
    m_redo.pop_back();
    for (Part& part : step.parts) part.part->Redo();
    NoteTouched(step);
    m_undo.push_back(std::move(step));
    Notify();
    return true;
}

bool Stack::TakeBack() {
    if (!Undo()) return false;
    m_redo.pop_back();
    Notify();
    return true;
}

void Stack::Forget(const Participant& p) {
    const auto touches = [&](const Step& s) { return std::ranges::any_of(s.parts, [&](const Part& part) { return part.owner == &p; }); };
    const std::size_t gone = std::erase_if(m_undo, touches) + std::erase_if(m_redo, touches);
    std::erase(m_open, &p);
    if (gone == 0) return;
    Notify();
}

std::vector<std::string> Stack::UndoLabels() const {
    std::vector<std::string> out;
    for (const Step& s : m_undo) out.push_back(s.label);
    return out;
}

std::vector<std::string> Stack::RedoLabels() const {
    std::vector<std::string> out;
    for (const Step& s : m_redo | std::views::reverse) out.push_back(s.label);
    return out;
}

void Stack::SetMaxSteps(std::size_t n) {
    m_maxSteps = std::max<std::size_t>(n, 1);
    if (m_undo.size() <= m_maxSteps) return;
    m_undo.erase(m_undo.begin(), m_undo.begin() + static_cast<std::ptrdiff_t>(m_undo.size() - m_maxSteps));
    Notify();
}

void Stack::NoteTouched(const Step& s) {
    m_lastTouched.clear();
    for (const Part& part : s.parts)
        for (Ref& ref : part.part->Touched())
            if (std::ranges::find(m_lastTouched, ref) == m_lastTouched.end()) m_lastTouched.push_back(std::move(ref));
}

void Stack::Notify() {
    for (const auto& l : m_listeners) l();
}

} // namespace undoer
