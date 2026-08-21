#!/bin/bash
set -euo pipefail

usage() {
    cat <<EOF
Usage: $0 [--build-dir DIR] [MESON_OPTIONS...]

Options:
  --build-dir DIR     Build directory (default: "build").
  -h, --help          Show this help.

Dependencies are resolved from system packages and Meson subprojects.
Remaining arguments are forwarded to meson setup.
EOF
}

build_folder="build"
meson_options=()

while [[ $# -gt 0 ]]; do
    case "$1" in
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

meson setup "$build_folder" "${meson_options[@]}"
meson compile -C "$build_folder"
