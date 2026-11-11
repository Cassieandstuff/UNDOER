#pragma once
/// @file
/// SnapshotHistory — undo by whole-document value snapshots, from sct-state's History<Doc> (the DAW's ScopedEdit + ProjectState
/// design), as a document on a Stack. For a document that is cheap to copy relative to how often it is edited (references and
/// metadata, not raw asset bytes); a big document of many parts wants ItemHistory instead.
///
/// The live model is bridged in by two callbacks: `capture()` snapshots it into a Doc value, `restore(doc)` replaces it from one.
/// Doc is a regular type: copyable and equality-comparable. A step records the document before and after only when they differ.
///
/// Drift guard: when the document joins a step it must be exactly as the last step left it. If it isn't, something changed it
/// outside an edit, and that change would silently not be undoable. The guard calls `onDrift` (or asserts, with none set); turn it
/// off while retrofitting an app whose edits aren't all bracketed yet.

#include <undoer/DocumentHistory.h>

#include <cassert>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace undoer {

template <class Doc>
class SnapshotHistory final : public DocumentHistory {
public:
    using Capture = std::function<Doc()>;
    using Restore = std::function<void(const Doc&)>;

    /// @param domain the Ref domain its steps name the document by: `{domain, address}`
    SnapshotHistory(Capture capture, Restore restore, std::string domain = {})
        : m_capture(std::move(capture)), m_restore(std::move(restore)), m_domain(std::move(domain)) {
        assert(m_capture && m_restore && "SnapshotHistory needs capture and restore");
        m_settled = m_capture();
    }

    /// @brief Report or ignore changes made outside an edit.
    SnapshotHistory& SetDriftGuard(bool on) {
        m_driftGuard = on;
        return *this;
    }
    /// @brief Called when the guard finds the document changed outside an edit; with none set, the guard asserts.
    std::function<void()> onDrift;

    /// @brief Run `fn()` as one undoable step.
    template <class Fn>
    void Apply(std::string label, Fn&& fn) {
        auto edit = Begin(std::move(label));
        fn();
    }

private:
    class SnapshotPart final : public Part {
    public:
        SnapshotPart(SnapshotHistory& owner, Doc before, Doc after) : Part(owner), m_history(owner), m_before(std::move(before)), m_after(std::move(after)) {}

        [[nodiscard]] std::vector<Ref> Touched() const override { return { { m_history.m_domain, m_history.Address(), {} } }; }

    protected:
        void Restore(bool undo) override {
            m_history.m_restore(undo ? m_before : m_after);
            m_history.m_settled = m_history.m_capture();   // restore may not reproduce the value byte for byte
        }

    private:
        SnapshotHistory& m_history;
        Doc              m_before, m_after;
    };

    void Joining() override {
        if (!m_driftGuard || m_capture() == m_settled) return;
        if (onDrift) onDrift();
        else assert(false && "undoer: the document changed outside an edit");
        m_settled = m_capture();   // what changed outside isn't undoable; measure from here
    }

    [[nodiscard]] std::unique_ptr<Part> CollectChanges() override {
        Doc now = m_capture();
        if (now == m_settled) return nullptr;
        auto part  = std::make_unique<SnapshotPart>(*this, std::move(m_settled), now);
        m_settled  = std::move(now);
        return part;
    }

    void Recapture() override { m_settled = m_capture(); }

    Capture     m_capture;
    Restore     m_restore;
    std::string m_domain;
    Doc         m_settled{};   ///< the document as the last step (or Reset) left it
    bool        m_driftGuard = true;
};

/// @brief A SnapshotHistory over a document held by value: capture and restore copy in and out of `doc`, which outlives it.
template <class Doc>
std::unique_ptr<SnapshotHistory<Doc>> MakeValueHistory(Doc& doc, std::string domain = {}) {
    return std::make_unique<SnapshotHistory<Doc>>([&doc] { return doc; }, [&doc](const Doc& d) { doc = d; }, std::move(domain));
}

} // namespace undoer
