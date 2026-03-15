from conan import ConanFile
from conan.tools.meson import MesonToolchain
from conan.tools.gnu import PkgConfigDeps

# ──────────────────────────────────────────────────────────────────────────
# Conan is OPTIONAL for balsa.  Core dependencies (eigen, fmt, spdlog,
# catch2, range-v3, nlohmann_json, glm, imgui, cli11) are resolved by
# Meson via system packages or WrapDB subprojects.
#
# Use Conan only when you need the heavier/harder-to-build dependencies
# listed below (Qt, Vulkan tool-chain, protobuf, etc.).
#
#   conan install . --output-folder=build/conan --build=missing
#   meson setup --native-file build/conan/conan_meson_native.ini build .
# ──────────────────────────────────────────────────────────────────────────

# option_name, option_values, default_value, conan_dependencies
__OPTIONAL_FLAGS_WITH_DEPS__ = [
    ("visualization", [True, False], True, [
        "shaderc/2021.1",
    ]),
    ("glfw",     [True, False], True,  ["glfw/3.3.8"]),
    ("imgui",    [True, False], True,  ["imgui/1.88"]),
    ("qt",       [True, False], True,  ["qt/6.8.3"]),
    ("protobuf", [True, False], True,  ["protobuf/5.27.0"]),
    ("openvdb",  [True, False], False, ["openvdb/11.0.0"]),
    ("alembic",  [True, False], True,  ["alembic/1.8.6"]),
    ("embree",   [True, False], False, ["embree3/3.13.5"]),
    ("perfetto", [True, False], False, ["perfetto/50.1"]),
    ("pngpp",    [True, False], False, ["pngpp/0.2.10"]),
    # Options with no Conan deps (handled entirely by Meson)
    ("json",   [True, False], True,  []),
    ("partio", [True, False], True,  []),
    ("igl",    [True, False], True,  []),
    ("eltopo", [True, False], True,  []),
]

__OPTIONS__ = {name: values for name, values, default, deps in __OPTIONAL_FLAGS_WITH_DEPS__}
__DEFAULT_OPTIONS__ = {name: default for name, values, default, deps in __OPTIONAL_FLAGS_WITH_DEPS__}


def dependencies(config):
    ret = set()
    for name, _, _, deps in __OPTIONAL_FLAGS_WITH_DEPS__:
        if getattr(config.options, name):
            ret.update(deps)
    return ret


class Balsa(ConanFile):
    settings = "os", "compiler", "build_type"
    options = __OPTIONS__
    default_options = __DEFAULT_OPTIONS__
    generators = "PkgConfigDeps"

    def requirements(self):
        for dep in dependencies(self):
            self.requires(dep)

        # Pin transitive dependency versions to avoid conflicts.
        self.requires("fmt/11.0.2", override=True)
        self.requires("abseil/20240722.0", override=True)
        self.requires("onetbb/2022.0.0", override=True)
        self.requires("boost/1.88.0", override=True)
        if self.options.visualization:
            self.requires("vulkan-headers/1.3.268.0", override=True)
            self.requires("vulkan-loader/1.3.268.0", override=True)
            self.requires("glslang/11.7.0", override=True)
            self.requires("spirv-tools/1.3.268.0", override=True)
            self.requires("spirv-headers/1.3.268.0", override=True)

    def configure(self):
        if self.options.visualization:
            self.options["glfw"].vulkan_static = True
            self.options["qt"].with_vulkan = True
        if self.options.imgui:
            self.options["imgui"].shared = True

    def generate(self):
        meson = MesonToolchain(self)
        for name, _, _, _ in __OPTIONAL_FLAGS_WITH_DEPS__:
            meson.project_options[name] = bool(getattr(self.options, name))
        meson.generate()
