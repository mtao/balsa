#!/bin/bash

#takes as input a single arg that is the meson profile name. otherwise it uses the default one
profile=${1}
if [[ -n "$profile" ]]; then
    conan_profile=$profile
    build_folder=build-$profile
else
    conan_profile=default
    build_folder=build
fi

mkdir -p $build_folder
conan install . --output-folder=$build_folder/conan --build=missing --profile=$conan_profile
pushd $build_folder
meson setup --native-file conan/conan_meson_native.ini .. .
ninja # build
