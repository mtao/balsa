#pragma once
#include <string>

namespace balsa::uri {

struct UriError {
    enum class Code {
        // The text is not a valid URI reference (or not resolvable against
        // the base).
        InvalidUri,
        // to_path() on a URI whose scheme is not file: or that has a host.
        NotAFileUri,
        // The decoded path is not usable (e.g. contains a NUL byte).
        InvalidPath,
        // DataUri::parse() on a URI that is not a well-formed data: URI.
        InvalidDataUri,
        // A base64 data: URI payload failed to decode.
        InvalidBase64,
        // Resolver::fetch() has no handler for the URI's scheme.
        UnsupportedScheme,
        // A handler could not read the resource (e.g. missing file).
        Io,
    };
    Code code;
    std::string message;
};

} // namespace balsa::uri
