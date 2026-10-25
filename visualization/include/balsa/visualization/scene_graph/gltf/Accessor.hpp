#pragma once
#include "Extension.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace balsa::visualization::scene_graph::gltf {

// The datatype of the accessor’s components.
enum class ComponentType : int64_t {
    BYTE = 5120,
    UNSIGNED_BYTE = 5121,
    SHORT = 5122,
    UNSIGNED_SHORT = 5123,
    UNSIGNED_INT = 5125,
    FLOAT = 5126,
};

// Specifies if the accessor’s elements are scalars, vectors, or matrices.
enum class AccessorType {
    SCALAR,
    VEC2,
    VEC3,
    VEC4,
    MAT2,
    MAT3,
    MAT4,
};

// An object pointing to a buffer view containing the indices of deviating
// accessor values. The number of indices is equal to accessor.sparse.count.
// Indices MUST strictly increase.
struct [[=object]] AccessorSparseIndices {
    // The index of the buffer view with sparse indices. The referenced buffer
    // view MUST NOT have its target or byteStride properties defined. The
    // buffer view and the optional byteOffset MUST be aligned to the
    // componentType byte length.
    [[=json::required]] int64_t bufferView;
    // The offset relative to the start of the buffer view in bytes.
    [[=json::skip_default]] int64_t byteOffset = 0;
    // The indices data type.
    [[=json::required]] ComponentType componentType;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

// An object pointing to a buffer view containing the deviating accessor values.
// The number of elements is equal to accessor.sparse.count times number of
// components. The elements have the same component type as the base accessor.
// The elements are tightly packed. Data MUST be aligned following the same
// rules as the base accessor.
struct [[=object]] AccessorSparseValues {
    // The index of the bufferView with sparse values. The referenced buffer
    // view MUST NOT have its target or byteStride properties defined.
    [[=json::required]] int64_t bufferView;
    // The offset relative to the start of the bufferView in bytes.
    [[=json::skip_default]] int64_t byteOffset = 0;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

// Sparse storage of accessor values that deviate from their initialization
// value.
struct [[=object]] AccessorSparse {
    // Number of deviating accessor values stored in the sparse array.
    [[=json::required]] int64_t count;
    // An object pointing to a buffer view containing the indices of deviating
    // accessor values. The number of indices is equal to count. Indices MUST
    // strictly increase.
    [[=json::required]] AccessorSparseIndices indices;
    // An object pointing to a buffer view containing the deviating accessor
    // values.
    [[=json::required]] AccessorSparseValues values;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

// A typed view into a buffer view that contains raw binary data.
struct [[=object]] Accessor {
    // The index of the bufferView.
    std::optional<int64_t> bufferView;
    // The offset relative to the start of the buffer view in bytes.
    [[=json::manual_write]] int64_t byteOffset = 0;
    // The datatype of the accessor’s components.
    [[=json::required]] ComponentType componentType;
    // Specifies whether integer data values are normalized before usage.
    [[=json::skip_default]] bool normalized = false;
    // The number of elements referenced by this accessor.
    [[=json::required]] int64_t count;
    // Specifies if the accessor’s elements are scalars, vectors, or matrices.
    [[=json::required]] AccessorType type;
    // Maximum value of each component in this accessor.
    std::vector<double> max;
    // Minimum value of each component in this accessor.
    std::vector<double> min;
    // Sparse storage of elements that deviate from their initialization value.
    std::optional<AccessorSparse> sparse;
    // The user-defined name of this object.
    std::optional<std::string> name;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

auto to_json(nlohmann::json &j, const AccessorType &v) -> void;
auto from_json(const nlohmann::json &j, AccessorType &v) -> void;
auto to_json(nlohmann::json &j, const AccessorSparseIndices &v) -> void;
auto from_json(const nlohmann::json &j, AccessorSparseIndices &v) -> void;
auto to_json(nlohmann::json &j, const AccessorSparseValues &v) -> void;
auto from_json(const nlohmann::json &j, AccessorSparseValues &v) -> void;
auto to_json(nlohmann::json &j, const AccessorSparse &v) -> void;
auto from_json(const nlohmann::json &j, AccessorSparse &v) -> void;
auto to_json(nlohmann::json &j, const Accessor &v) -> void;
auto from_json(const nlohmann::json &j, Accessor &v) -> void;

} // namespace balsa::visualization::scene_graph::gltf
