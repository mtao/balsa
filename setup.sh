#!/bin/bash
set -euo pipefail

usage() {
    cat <<EOF
Usage: $0 [--conan [profile]] [--build-dir DIR] [MESON_OPTIONS...]

Options:
  --conan [profile]   Resolve dependencies with Conan (default profile: default).
  --build-dir DIR     Build directory (default: "build").
  -h, --help          Show this help.

Dependencies are resolved from system packages and Meson subprojects.
Remaining arguments are forwarded to meson setup.
EOF
}

build_folder="build"
use_conan=false
conan_profile="default"
meson_options=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --conan)
            use_conan=true
            if [[ -n "${2:-}" && "${2:0:1}" != "-" ]]; then
                conan_profile="$2"
                shift
            fi
            ;;
        --build-dir)
            if [[ -z "${2:-}" ]]; then
                echo "--build-dir requires a value" >&2
                exit 1
            fi
            build_folder="$2"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            meson_options+=("$1")
            ;;
    esac
    shift
done

if $use_conan; then
    conan install . \
        --output-folder="$build_folder/conan" \
        --build=missing \
        --profile="$conan_profile"
    meson_options+=("--native-file" "$build_folder/conan/conan_meson_native.ini")
fi

meson setup "$build_folder" "${meson_options[@]}"
meson compile -C "$build_folder"
