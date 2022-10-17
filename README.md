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
pushd build # enter build directory
conan install .. --build-missing # pull dependencies, build as needed
conan build .. --build-folder=. # set up meson and build
```
Subsequent build can be done with Ninja.
