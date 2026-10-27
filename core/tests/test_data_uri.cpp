#include <catch2/catch_all.hpp>

#include <balsa/encoding/base64.hpp>
#include <balsa/uri/DataUri.hpp>

#include <string_view>

using balsa::uri::DataUri;
using balsa::uri::Uri;
using balsa::uri::UriError;

namespace {
auto as_bytes(std::string_view text) -> std::vector<std::byte> {
    const auto *begin = reinterpret_cast<const std::byte *>(text.data());
    return {begin, begin + text.size()};
}
} // namespace

TEST_CASE("base64_round_trip", "[encoding][base64]") {
    using namespace balsa::encoding;
    for (std::string_view text : {"", "f", "fo", "foo", "foob", "fooba",
                                  "foobar"}) {
        const auto encoded = base64_encode(as_bytes(text));
        const auto decoded = base64_decode(encoded);
        REQUIRE(decoded);
        CHECK(*decoded == as_bytes(text));
    }
    // RFC 4648 §10 test vectors.
    CHECK(base64_encode(as_bytes("foobar")) == "Zm9vYmFy");
    CHECK(base64_encode(as_bytes("fo")) == "Zm8=");
}

TEST_CASE("base64_decode_strictness", "[encoding][base64]") {
    using namespace balsa::encoding;
    // Whitespace is ignored.
    CHECK(base64_decode("Zm9v\nYmFy") == as_bytes("foobar"));

    CHECK_FALSE(base64_decode("Zm9v!"));     // bad character
    CHECK_FALSE(base64_decode("Zm8"));       // missing padding
    CHECK_FALSE(base64_decode("Zm9vY"));     // 1-char final chunk
    CHECK_FALSE(base64_decode("Zm9="));      // non-zero trailing bits
    CHECK_FALSE(base64_decode("Zm=9vYmFy")); // padding mid-stream

    const auto error = base64_decode("Zm9v!");
    REQUIRE_FALSE(error);
    CHECK(error.error().position == 4);
}

TEST_CASE("data_uri_parse", "[uri][data_uri]") {
    const auto uri =
        Uri::parse("data:application/octet-stream;base64,Zm9vYmFy#frag");
    REQUIRE(uri);
    const auto data = DataUri::parse(*uri);
    REQUIRE(data);
    CHECK(uri->body() == "application/octet-stream;base64,Zm9vYmFy");
    CHECK(data->media_type == "application/octet-stream");
    CHECK(data->is_base64);
    CHECK(data->payload == "Zm9vYmFy");
    CHECK(data->decode() == as_bytes("foobar"));

    // Parameters stay in the media type; ;base64 is case-insensitive.
    const auto typed = Uri::parse("data:text/plain;charset=utf-8;BASE64,Zm8=");
    REQUIRE(typed);
    const auto typed_data = DataUri::parse(*typed);
    REQUIRE(typed_data);
    CHECK(typed_data->media_type == "text/plain;charset=utf-8");
    CHECK(typed_data->is_base64);

    // No media type, percent-encoded payload.
    const auto plain = Uri::parse("data:,hello%20world");
    REQUIRE(plain);
    const auto plain_data = DataUri::parse(*plain);
    REQUIRE(plain_data);
    CHECK(plain_data->media_type.empty());
    CHECK_FALSE(plain_data->is_base64);
    CHECK(plain_data->decode() == as_bytes("hello world"));
}

TEST_CASE("data_uri_errors", "[uri][data_uri]") {
    const auto https = Uri::parse("https://example.com/x");
    REQUIRE(https);
    REQUIRE_FALSE(DataUri::parse(*https));
    CHECK(DataUri::parse(*https).error().code
          == UriError::Code::InvalidDataUri);

    const auto no_comma = Uri::parse("data:text/plain;base64");
    REQUIRE(no_comma);
    CHECK_FALSE(DataUri::parse(*no_comma));

    const auto bad = Uri::parse("data:;base64,Zm9vY");
    REQUIRE(bad);
    const auto bad_data = DataUri::parse(*bad);
    REQUIRE(bad_data);
    REQUIRE_FALSE(bad_data->decode());
    CHECK(bad_data->decode().error().code == UriError::Code::InvalidBase64);
}
