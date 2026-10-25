#pragma once
// Reflection-driven nlohmann::json (de)serialization. Heavy (includes
// nlohmann/json.hpp): include it from .cpp files; headers declaring annotated
// types only need balsa/json/annotations.hpp.
//
// Requires C++26 static reflection with annotations (P2996 + P3394).
#include <version>
#if !defined(__cpp_impl_reflection) || !defined(__cpp_lib_reflection)
#error "balsa/json/reflection.hpp requires C++26 reflection"
#endif

#include "annotations.hpp"
#include "raw_json.hpp"

#include <nlohmann/json.hpp>

#include <meta>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

// C++26 makes std::optional a range, which sends nlohmann's built-in
// optional support down its container path. Handle it explicitly.
template <typename T>
struct nlohmann::adl_serializer<std::optional<T>> {
    static void to_json(json &j, const std::optional<T> &opt) {
        if (opt) {
            j = *opt;
        } else {
            j = nullptr;
        }
    }
    static void from_json(const json &j, std::optional<T> &opt) {
        if (j.is_null()) {
            opt.reset();
        } else {
            opt = j.get<T>();
        }
    }
};

namespace balsa::json {

// to_json / from_json for RawJson: parse on write, serialize on read.
inline auto to_json(nlohmann::json &j, const RawJson &raw) -> void {
    j = nlohmann::json::parse(raw.text);
}
inline auto from_json(const nlohmann::json &j, RawJson &raw) -> void {
    raw.text = j.dump();
}

namespace detail {
    template <typename T>
    concept emptiable = requires(const T &v) { v.has_value(); }
                        || requires(const T &v) { v.empty(); };

    // Unset optionals, empty containers/strings and null or empty JSON values.
    template <emptiable T>
    constexpr auto is_empty(const T &value) -> bool {
        if constexpr (requires { value.has_value(); }) {
            return !value.has_value();
        } else {
            return value.empty();
        }
    }

    template <typename T, typename U = T>
        requires std::is_convertible_v<U, T>
    auto from_with_default(const nlohmann::json &j,
                           std::string_view key,
                           T &value,
                           const U &default_value) -> void {
        if (auto it = j.find(key); it != j.end()) {
            it->get_to(value);
        } else {
            value = default_value;
        }
    }

    // Annotation of type A on `entity`, if any; at most one is allowed.
    template <typename A>
    consteval auto annotation(std::meta::info entity) -> std::optional<A> {
        auto found = std::meta::annotations_of_with_type(entity, ^^A);
        if (found.empty()) { return std::nullopt; }
        if (found.size() > 1) {
            throw std::meta::exception(
                u8"multiple balsa::json annotations on one entity", entity);
        }
        return std::meta::extract<A>(found.front());
    }

    // The member's own policy, else its type's default, else required.
    consteval auto policy_of(std::meta::info type, std::meta::info member)
        -> policy_kind {
        if (auto p = annotation<policy>(member)) { return p->kind; }
        if (auto m = annotation<members_t>(type)) {
            return m->default_policy.kind;
        }
        return policy_kind::required;
    }

    // Write member `m` of `v` under policy `kind`.
    template <policy_kind kind, std::meta::info m, typename T>
    auto write_member(nlohmann::json &j, const T &v) -> void {
        static_assert(std::meta::parent_of(m) == ^^T,
                      "member does not belong to this type");
        constexpr std::string_view key = std::meta::identifier_of(m);
        if constexpr (kind == policy_kind::required) {
            j[key] = v.[:m:];
        } else if constexpr (kind == policy_kind::omit_empty) {
            static_assert(detail::emptiable<typename[:std::meta::type_of(m):]>,
                          "json::omit_empty members must be optional or have "
                          "empty(); use json::required or json::skip_default");
            if (!detail::is_empty(v.[:m:])) { j[key] = v.[:m:]; }
        } else if constexpr (kind == policy_kind::skip_default) {
            static const T default_value{};
            if (!(v.[:m:] == default_value.[:m:])) { j[key] = v.[:m:]; }
        }
    }
} // namespace detail

template <typename T>
auto to_json_members(nlohmann::json &j, const T &v) -> void {
    static constexpr auto data_members =
        std::define_static_array(std::meta::nonstatic_data_members_of(
            ^^T, std::meta::access_context::unprivileged()));

    template for (constexpr auto m : data_members) {
        detail::write_member<detail::policy_of(^^T, m), m>(j, v);
    }
}

// Write only the given members, e.g. from code handling json::manual_write
// members: `json::write_members<json::skip_default, ^^Node::scale>(j, v);`.
// Without a policy the members are always written.
template <policy P, std::meta::info... Members, typename T>
    requires(P.kind != policy_kind::manual_write)
auto write_members(nlohmann::json &j, const T &v) -> void {
    (detail::write_member<P.kind, Members>(j, v), ...);
}
template <std::meta::info... Members, typename T>
auto write_members(nlohmann::json &j, const T &v) -> void {
    write_members<required, Members...>(j, v);
}

template <typename T>
auto from_json_members(const nlohmann::json &j, T &v) -> void {
    static const T default_value{};
    static constexpr auto data_members =
        std::define_static_array(std::meta::nonstatic_data_members_of(
            ^^T, std::meta::access_context::unprivileged()));
    template for (constexpr auto m : data_members) {
        constexpr std::string_view key = std::meta::identifier_of(m);
        if constexpr (detail::policy_of(^^T, m) == policy_kind::required) {
            j.at(key).get_to(v.[:m:]);
        } else {
            detail::from_with_default(j, key, v.[:m:], default_value.[:m:]);
        }
    }
}

// Serialize an enum as its enumerator name.
template <typename E>
    requires std::is_enum_v<E>
auto enum_to_json(nlohmann::json &j, const E &e) -> void {
    template for (constexpr auto en :
                  std::define_static_array(std::meta::enumerators_of(^^E))) {
        if (e == [:en:]) {
            j = std::meta::identifier_of(en);
            return;
        }
    }
    throw std::invalid_argument(
        "enum_to_json: value is not a named enumerator");
}

template <typename E>
    requires std::is_enum_v<E>
auto enum_from_json(const nlohmann::json &j, E &e) -> void {
    const auto &name = j.get_ref<const std::string &>();
    template for (constexpr auto en :
                  std::define_static_array(std::meta::enumerators_of(^^E))) {
        if (name == std::meta::identifier_of(en)) {
            e = [:en:];
            return;
        }
    }
    throw std::invalid_argument("enum_from_json: unknown enumerator '" + name
                                + "'");
}

} // namespace balsa::json
