#pragma once
// Unparsed JSON text, for values whose structure is open-ended (e.g. glTF
// extensions/extras). Lightweight: no JSON library is included; conversion
// to and from parsed JSON lives in balsa/json/reflection.hpp.
#include <string>

namespace balsa::json {

struct RawJson {
    // Serialized JSON value; empty means absent.
    std::string text;

    auto empty() const -> bool { return text.empty(); }
    auto operator==(const RawJson &) const -> bool = default;
};

} // namespace balsa::json
