# balsa
The lightweight base material for building things.

This is a cleanup of [core](https://github.com/mtao/core) with a less egotistical namespace, a focus on Qt and Vulkan, and using meson to clean up the build process. The visualization / scenegraph system borrows heavily from [magnum](https://magnum.graphics) - I'm trying do a concept-heavy vulkan-focused variant of it.

## Building
Dependencies are resolved from system packages and Meson subprojects.

```bash
python -m pip install meson ninja
meson setup build
meson compile -C build
meson test -C build -v
```

The `setup.sh` helper configures and builds the default `build` directory.
Use `./setup.sh --build-dir build-debug` to select another directory.

Conan is optional. To resolve available dependencies through Conan instead:

```bash
python -m pip install conan
./setup.sh --conan --build-dir build-conan
```
