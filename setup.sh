#!/bin/bash
set -euo pipefail

usage() {
    cat <<EOF
Usage: $0 [--conan [profile]] [--build-dir DIR]

Options:
  --conan [profile]   Run conan install before meson setup (default profile: "default").
                      Use this for heavy deps (Qt, Vulkan, protobuf, …).
  --build-dir DIR     Build directory (default: "build", or "build-<profile>" with --conan).
  -h, --help          Show this help.

Without --conan, dependencies are resolved from system packages and
Meson WrapDB subprojects.  No package manager required.
EOF
}

use_conan=false
conan_profile="default"
build_folder=""

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
            build_folder="$2"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
    shift
done

# Default build dir
if [[ -z "$build_folder" ]]; then
    if $use_conan && [[ "$conan_profile" != "default" ]]; then
        build_folder="build-$conan_profile"
    else
        build_folder="build"
    fi
fi

mkdir -p "$build_folder"

if $use_conan; then
    echo "── conan install (profile: $conan_profile) ──"
    conan install . --output-folder="$build_folder/conan" --build=missing --profile="$conan_profile"
    pushd "$build_folder"
    meson setup --native-file conan/conan_meson_native.ini .. .
    popd
else
    echo "── meson setup (system + subprojects, no conan) ──"
    meson setup "$build_folder" .
fi

ninja -C "$build_folder"
