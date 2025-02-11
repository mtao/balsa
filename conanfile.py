from conan import ConanFile
from conan.tools.meson import MesonToolchain
from conan.tools.gnu import PkgConfigDeps
import os

__BASE_DEPS__ = [
        "spdlog/1.14.1", 
        "eigen/3.4.0",
        "range-v3/cci.20240905",  
        # "abseil/20240722.0",
        # "fmt/11.0.2", 
        "onetbb/2021.10.0",  # constrained by openvdb
        "catch2/3.7.1",
        "cxxopts/3.2.0",
        ]


# option_name, option_values, default_value, dependencies
__OPTIONAL_FLAGS_WITH_DEPS__ = [
            ("visualization", [True,False], True, [
                "shaderc/2021.1", "glfw/3.3.8", "glm/0.9.9.8", "imgui/1.88"
                ]),
            ("openvdb", [True,False], True,
                ["openvdb/11.0.0"]
                ),
            ("protobuf", [True,False], True,
                ["protobuf/5.27.0"]
                ),
            ("json", [True,False], True,
                ["nlohmann_json/3.11.2"]
                ),
            ("embree", [True,False], False,
                ["embree3/3.13.5"]
                ),
            ("imgui", [True,False], True,
                ["imgui/1.88"]
                ),
            ("pngpp", [True,False], False,
                ["pngpp/0.2.10"]
                ),
            ("qt", [True,False], True,
                ["qt/6.7.3"]
                ),
            # These are libs that i want options for but meson has to handle
            ("partio", [True,False], True,[]),
            ("igl", [True,False], True,[]),
            ("eltopo", [True,False], True,[]),
            ("alembic", [True,False], True,['alembic/1.8.6']),
            ]


__OPTIONS__ = {name:values for name,values,default,deps in __OPTIONAL_FLAGS_WITH_DEPS__}

__DEFAULT_OPTIONS__ = default_options = {name:default for name,values,default,deps in __OPTIONAL_FLAGS_WITH_DEPS__}


def dependencies(config):
    ret = set()
    for name, _, _, deps in __OPTIONAL_FLAGS_WITH_DEPS__:
        if getattr(config.options,name):
            ret.update(deps)
    return ret


class Balsa(ConanFile):
    requires = __BASE_DEPS__ 
    settings = "os", "compiler", "build_type"  
    options = __OPTIONS__
    default_options = __DEFAULT_OPTIONS__

    generators = "PkgConfigDeps"


    def requirements(self):
        for dep in dependencies(self):
            self.requires(dep)

        #  make sure fmt is the version we want
        self.requires("fmt/11.0.2", override=True)
        self.requires("abseil/20240722.0", override=True)
        if self.options.visualization:
            # glfw and qt overlap sadly
            self.requires("vulkan-headers/1.3.268.0",override=True)
            self.requires("vulkan-loader/1.3.268.0",override=True)
            self.requires("glslang/11.7.0",override=True)
            self.requires("spirv-tools/1.3.268.0",override=True)
            self.requires("spirv-headers/1.3.268.0",override=True)
                                               
    def configure(self):
        if self.options.visualization:
            self.options["glfw"].vulkan_static = True
            self.options["qt"].with_vulkan = True
             
    def generate(self):
        meson = MesonToolchain(self)                   
        args = []
        for name, _, _, _ in __OPTIONAL_FLAGS_WITH_DEPS__:
            value = getattr(self.options,name)
            meson.project_options[name] =  value
        meson.generate()

                                               
