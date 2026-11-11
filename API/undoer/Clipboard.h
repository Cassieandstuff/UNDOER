#pragma once
/// @file
/// Clipboard — a type-erased, single-slot clipboard, from sct-state. Copy stores a value of some payload type; paste is offered
/// only when the stored type matches, which is what makes Paste context-aware: a command offers Paste<T> only while Has<T>().

#include <any>
#include <typeindex>
#include <typeinfo>
#include <utility>

namespace undoer {

class Clipboard {
public:
    template <class T>
    void Set(T value) {
        m_payload = std::move(value);
        m_type    = std::type_index(typeid(T));
    }

    template <class T>
    [[nodiscard]] bool Has() const {
        return m_payload.has_value() && m_type == std::type_index(typeid(T));
    }

    /// @return null unless the stored payload is exactly a T
    template <class T>
    [[nodiscard]] const T* Get() const {
        return Has<T>() ? std::any_cast<T>(&m_payload) : nullptr;
    }

    [[nodiscard]] bool Empty() const noexcept { return !m_payload.has_value(); }
    void Clear() {
        m_payload.reset();
        m_type = std::type_index(typeid(void));
    }

private:
    std::any        m_payload;
    std::type_index m_type{ std::type_index(typeid(void)) };
};

} // namespace undoer
