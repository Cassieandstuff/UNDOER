#pragma once
/// ItemHistory — undo for a document that is a list of value items: a NIF's blocks, a scene's placements, a material's one record.
/// An edit says which items it is about to change (Touch), or that it changes the list itself (TouchAll); the first touch in an
/// open edit snapshots the item, and when the edit closes only the items that really differ become the step. A step therefore holds
/// the changed items before and after, never the whole document, unless the list itself changed.
//
/// Touch outside an open edit records nothing: change the document only inside `history.Begin(...)` or `Apply(...)`.

#include <undoer/DocumentHistory.h>

#include <concepts>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace undoer {

template <class Item>
class ItemHistory final : public DocumentHistory {
public:
    /// @brief Whether two states of an item are the same; a touched item that compares the same is left out of the step.
    using Same = std::function<bool(const Item&, const Item&)>;
    /// @brief The Ref a step names item `i` by (Touched).
    using RefOf = std::function<Ref(std::size_t)>;

    /// @param items the document's items; they outlive the history
    /// @param domain the Ref domain of what it names: item `i` is `{domain, "<address>#<i>"}`, the list `{domain, "<address>"}`
    ItemHistory(std::vector<Item>& items, std::string domain, Same same = {}) : m_items(items), m_domain(std::move(domain)), m_same(std::move(same)) {
        if (!m_same) {
            if constexpr (std::equality_comparable<Item>) m_same = [](const Item& a, const Item& b) { return a == b; };
            else m_same = [](const Item&, const Item&) { return false; };
        }
    }

    /// @brief Name item `i` differently in steps (default: `{domain, "<address>#<i>"}`).
    void SetRefOf(RefOf refOf) { m_refOf = std::move(refOf); }

    using DocumentHistory::Touch;

    /// @brief Item `i` is about to change in the open edit.
    void Touch(std::size_t i) {
        if (!GetStack().Open()) return;
        DocumentHistory::Touch();
        if (m_whole || i >= m_items.size()) return;
        m_before.try_emplace(i, m_items[i]);
    }
    /// @brief The list itself is about to change: items added, removed or reordered.
    void TouchAll() {
        if (!GetStack().Open()) return;
        DocumentHistory::Touch();
        if (m_whole) return;
        // Items touched earlier in the edit go back to their state at its start.
        std::vector<Item> start = m_items;
        for (auto& [i, item] : m_before) start[i] = item;
        m_before.clear();
        m_whole = std::move(start);
    }

    /// @brief Run `fn(items)` as one undoable step. `fn` still says what it changes (Touch, TouchAll).
    template <class Fn>
    void Apply(std::string label, Fn&& fn) {
        auto edit = Begin(std::move(label));
        fn(m_items);
    }

    [[nodiscard]] Ref ItemRef(std::size_t i) const {
        if (m_refOf) return m_refOf(i);
        return { m_domain, Address() + "#" + std::to_string(i), {} };
    }

private:
    class ItemPart final : public Part {
    public:
        explicit ItemPart(ItemHistory& owner) : Part(owner), m_history(owner) {}

        struct Change {
            std::size_t index = 0;
            Item        before, after;
        };
        std::vector<Change>                                      changes;
        std::optional<std::pair<std::vector<Item>, std::vector<Item>>> whole;   ///< the list before and after

        [[nodiscard]] std::vector<Ref> Touched() const override {
            std::vector<Ref> out;
            if (whole) out.push_back({ m_history.m_domain, m_history.Address(), {} });
            for (const Change& c : changes) out.push_back(m_history.ItemRef(c.index));
            return out;
        }

    protected:
        void Restore(bool undo) override {
            if (whole) m_history.m_items = undo ? whole->first : whole->second;
            for (const Change& c : changes)
                if (c.index < m_history.m_items.size()) m_history.m_items[c.index] = undo ? c.before : c.after;
        }

    private:
        ItemHistory& m_history;
    };

    [[nodiscard]] std::unique_ptr<Part> CollectChanges() override {
        auto part = std::make_unique<ItemPart>(*this);
        if (m_whole) {
            const bool same = m_whole->size() == m_items.size() && [&] {
                for (std::size_t i = 0; i < m_items.size(); ++i)
                    if (!m_same((*m_whole)[i], m_items[i])) return false;
                return true;
            }();
            if (!same) part->whole.emplace(std::move(*m_whole), m_items);
        }
        for (auto& [i, before] : m_before)
            if (i < m_items.size() && !m_same(before, m_items[i])) part->changes.push_back({ i, std::move(before), m_items[i] });
        m_whole.reset();
        m_before.clear();
        if (!part->whole && part->changes.empty()) return nullptr;
        return part;
    }

    void Recapture() override {
        m_whole.reset();
        m_before.clear();
    }

    std::vector<Item>&               m_items;
    std::string                      m_domain;
    Same                             m_same;
    RefOf                            m_refOf;
    std::map<std::size_t, Item>      m_before;   ///< the items the open edit touched, as they were when it first touched them
    std::optional<std::vector<Item>> m_whole;    ///< the list at the start of an open edit that changes the list itself
};

} // namespace undoer
