#pragma once
/// @file
/// The address a step names what it changed by: a domain, a stable id inside it, and a field path inside that object. UNDOER
/// never reads one; it hands them back (Stack::LastTouched) so an app can show or go to what an undo changed. Names are
/// byte-exact.

#include <string>

namespace undoer {

struct Ref {
    std::string domain;
    std::string id;
    std::string path;   ///< "" = the object itself

    [[nodiscard]] bool Empty() const { return domain.empty(); }
    [[nodiscard]] Ref  Field(std::string fieldPath) const { return { domain, id, std::move(fieldPath) }; }
    bool operator==(const Ref&) const = default;
};

} // namespace undoer
