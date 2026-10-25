#pragma once
// Member/type annotations for balsa's reflection-driven JSON
// (balsa/json/reflection.hpp). Lightweight: no JSON library is included, so
// headers declaring annotated types can include this cheaply.
//
// Applying annotations (`[[=json::required]]`) requires C++26 reflection
// annotations (P2996 + P3394)
#include <version>
#if !defined(__cpp_impl_reflection) || !defined(__cpp_lib_reflection)
#error "balsa/json/annotations.hpp requires C++26 reflection"
#endif

namespace balsa::json {

// How a member is written and read. Annotate a member to pick its policy,
// e.g. `[[=json::omit_empty]] std::vector<int> ids;`, or annotate a type to
// change the policy of its unannotated members, e.g.
// `struct [[=json::members(json::omit_empty)]] Foo { ... };`.
// Unannotated members of unannotated types are json::required.
enum class policy_kind {
    // Always written; reading throws json::out_of_range when missing.
    required,
    // Skipped on write when empty (unset optional, empty container/string,
    // empty RawJson); missing on read means default-constructed.
    omit_empty,
    // Skipped on write when equal to the value in a default-constructed
    // object; missing on read means that default.
    skip_default,
    // Not written by to_json_members; the caller writes it by hand (e.g.
    // "only one of" rules). Missing on read means default-constructed.
    manual_write,
};

struct policy {
    policy_kind kind;
};
inline constexpr policy required{policy_kind::required};
inline constexpr policy omit_empty{policy_kind::omit_empty};
inline constexpr policy skip_default{policy_kind::skip_default};
inline constexpr policy manual_write{policy_kind::manual_write};

// Type annotation setting the default policy of its unannotated members.
struct members_t {
    policy default_policy;
};
consteval auto members(policy p) -> members_t { return {p}; }

} // namespace balsa::json
