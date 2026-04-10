#include "balsa/terminal/image_output.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <string>

namespace balsa::terminal {

// ── Base64 encoder (RFC 4648) ──────────────────────────────────────────────

namespace detail {

    auto base64_encode(std::span<const uint8_t> data) -> std::string {
        static constexpr char table[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string out;
        out.reserve(((data.size() + 2) / 3) * 4);

        size_t i = 0;
        for (; i + 2 < data.size(); i += 3) {
            uint32_t triple = (uint32_t(data[i]) << 16)
                              | (uint32_t(data[i + 1]) << 8)
                              | uint32_t(data[i + 2]);
            out.push_back(table[(triple >> 18) & 0x3F]);
            out.push_back(table[(triple >> 12) & 0x3F]);
            out.push_back(table[(triple >> 6) & 0x3F]);
            out.push_back(table[triple & 0x3F]);
        }
        if (i + 1 == data.size()) {
            uint32_t val = uint32_t(data[i]) << 16;
            out.push_back(table[(val >> 18) & 0x3F]);
            out.push_back(table[(val >> 12) & 0x3F]);
            out.push_back('=');
            out.push_back('=');
        } else if (i + 2 == data.size()) {
            uint32_t val =
                (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8);
            out.push_back(table[(val >> 18) & 0x3F]);
            out.push_back(table[(val >> 12) & 0x3F]);
            out.push_back(table[(val >> 6) & 0x3F]);
            out.push_back('=');
        }
        return out;
    }

} // namespace detail

// ── Detection ──────────────────────────────────────────────────────────────

auto detect_kitty_support() -> bool {
    // Kitty graphics protocol is supported by Kitty, Ghostty, and WezTerm.
    const char *term_program = std::getenv("TERM_PROGRAM");
    if (term_program) {
        std::string tp(term_program);
        if (tp == "ghostty" || tp == "WezTerm") { return true; }
    }
    // Also check TERM for kitty (kitty sets TERM=xterm-kitty)
    const char *term = std::getenv("TERM");
    if (term) {
        std::string t(term);
        if (t.find("kitty") != std::string::npos) { return true; }
    }
    return false;
}

// ── Kitty graphics protocol ────────────────────────────────────────────────

void emit_kitty(size_t width,
                size_t height,
                std::span<const uint8_t> rgba8,
                FILE *fp) {
    // Encode the raw RGBA pixel data as base64.
    auto b64 = detail::base64_encode(rgba8);

    // Kitty protocol: chunk the payload into pieces of up to 4096 bytes.
    // First chunk carries the image metadata; subsequent chunks carry
    // only the continuation flag (m=1) or final flag (m=0).
    constexpr size_t chunk_size = 4096;
    size_t offset = 0;
    bool first = true;

    while (offset < b64.size()) {
        size_t remaining = b64.size() - offset;
        size_t n = std::min(remaining, chunk_size);
        bool more = (offset + n < b64.size());

        if (first) {
            // f=32 : 32-bit RGBA pixels
            // s=W  : width in pixels
            // v=H  : height in pixels
            // a=T  : action = transmit and display
            // t=d  : transmission = direct (inline data)
            // m=1/0: more chunks / last chunk
            std::fprintf(fp,
                         "\033_Gf=32,s=%zu,v=%zu,a=T,t=d,m=%d;",
                         width,
                         height,
                         more ? 1 : 0);
            first = false;
        } else {
            std::fprintf(fp, "\033_Gm=%d;", more ? 1 : 0);
        }
        std::fwrite(b64.data() + offset, 1, n, fp);
        std::fprintf(fp, "\033\\");
        offset += n;
    }

    // Newline after the image so subsequent text appears below.
    std::fprintf(fp, "\n");
    std::fflush(fp);
}

// ── Half-block truecolor ───────────────────────────────────────────────────

void emit_halfblock(size_t width,
                    size_t height,
                    std::span<const uint8_t> rgba8,
                    FILE *fp) {
    // Process rows in pairs. For each pair, the top pixel is the foreground
    // color and the bottom pixel is the background color of a "▀" (U+2580).
    // If height is odd, the last row gets a black bottom pixel.

    auto px = [&](size_t x, size_t y) -> std::array<uint8_t, 3> {
        if (y >= height) { return {0, 0, 0}; }
        size_t idx = (y * width + x) * 4;
        return {rgba8[idx], rgba8[idx + 1], rgba8[idx + 2]};
    };

    for (size_t y = 0; y < height; y += 2) {
        for (size_t x = 0; x < width; ++x) {
            auto [tr, tg, tb] = px(x, y);
            auto [br, bg, bb] = px(x, y + 1);
            // ESC[38;2;r;g;bm  = set foreground (top pixel)
            // ESC[48;2;r;g;bm  = set background (bottom pixel)
            std::fprintf(fp,
                         "\033[38;2;%u;%u;%u;48;2;%u;%u;%um\xe2\x96\x80",
                         tr,
                         tg,
                         tb,
                         br,
                         bg,
                         bb);
        }
        // Reset attributes and newline.
        std::fprintf(fp, "\033[0m\n");
    }
    std::fflush(fp);
}

// ── Auto-detect ────────────────────────────────────────────────────────────

void emit_auto(size_t width,
               size_t height,
               std::span<const uint8_t> rgba8,
               FILE *fp) {
    if (detect_kitty_support()) {
        emit_kitty(width, height, rgba8, fp);
    } else {
        emit_halfblock(width, height, rgba8, fp);
    }
}

} // namespace balsa::terminal
