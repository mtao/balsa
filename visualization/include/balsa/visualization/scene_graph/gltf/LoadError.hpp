#pragma once
#include <string>

namespace balsa::visualization::gltf {

struct LoadError {
    enum class Code {
        // A file could not be read.
        Io,
        // The JSON is malformed.
        InvalidJson,
        // The GLB container is malformed.
        InvalidGlb,
        // The JSON does not match the glTF schema (e.g. a missing required
        // property or an unknown enum string).
        InvalidGltf,
        // asset.version / asset.minVersion is not supported (only 2.0 is).
        UnsupportedVersion,
        // extensionsRequired lists an extension this loader does not support.
        UnsupportedExtension,
        // Referenced data is missing, unresolvable or shorter than declared.
        MissingData,
    };
    Code code;
    std::string message;
};

} // namespace balsa::visualization::gltf
