# balsa
The lightweight base material for building things.

This is a cleanup of [core](https://github.com/mtao/core) with a less egotistical namespace, a focus on Qt and Vulkan, and using meson to clean up the build process. The visualization / scenegraph system borrows heavily from [magnum](https://magnum.graphics) - I'm trying do a concept-heavy vulkan-focused variant of it.

## Building
This package is build using `meson` and dependencies can be installed using
`conan`. The easiest way to build it to first install those two with pip:
```bash
    pip install conan meson

```bash
mkdir build # make build directory
conan install .. --output-folder=build/_conan_pkgs --build-missing # pull dependencies, build as needed
pushd build # enter build directory
PKG_CONFIG_PATH=_conan_pkgs meson setup --native-file ../conan_meson_native.ini # use conan packages and build
```
Subsequent build can be done with Ninja.
