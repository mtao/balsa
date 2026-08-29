#pragma once

/// @file image_output.hpp
/// @brief Terminal image output via Kitty graphics protocol or half-block
///        truecolor characters.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>

namespace balsa::terminal {

/// Detect whether the current terminal likely supports the Kitty graphics
/// protocol (Kitty, Ghostty, WezTerm).
auto detect_kitty_support() -> bool;

/// Emit an RGBA8 image via the Kitty graphics protocol.
/// @param width   Image width in pixels.
/// @param height  Image height in pixels.
/// @param rgba8   Pixel data: width*height*4 bytes, row-major RGBA8.
/// @param fp      Output file (default: stdout).
void emit_kitty(size_t width,
                size_t height,
                std::span<const uint8_t> rgba8,
                FILE *fp = stdout);

/// Emit an RGBA8 image using Unicode half-block characters (U+2580 "▀")
/// with 24-bit ANSI truecolor escape sequences.
/// @param width   Image width in pixels.
/// @param height  Image height in pixels.
/// @param rgba8   Pixel data: width*height*4 bytes, row-major RGBA8.
/// @param fp      Output file (default: stdout).
void emit_halfblock(size_t width,
                    size_t height,
                    std::span<const uint8_t> rgba8,
                    FILE *fp = stdout);

/// Auto-detect terminal capabilities and emit the image using the best
/// available method (Kitty if supported, otherwise half-block).
/// @param width   Image width in pixels.
/// @param height  Image height in pixels.
/// @param rgba8   Pixel data: width*height*4 bytes, row-major RGBA8.
/// @param fp      Output file (default: stdout).
void emit_auto(size_t width,
               size_t height,
               std::span<const uint8_t> rgba8,
               FILE *fp = stdout);

} // namespace balsa::terminal
