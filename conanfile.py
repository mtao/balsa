from conan import ConanFile
from conan.tools.meson import MesonToolchain
import os

__BASE_DEPS__ = [
        "spdlog/1.14.1", 
        "eigen/3.4.0",
        "range-v3/cci.20240905",  
        "fmt/10.2.1", 
        "onetbb/2021.10.0",  # openvdb 8.1.0 requires this 
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
            ("embree", [True,False], True,
                ["embree3/3.13.5"]
                ),
            ("imgui", [True,False], True,
                ["imgui/1.88"]
                ),
            ("pngpp", [True,False], False,
                ["pngpp/0.2.10"]
                ),
            ("qt", [True,False], False,
                ["qt/5.15.8"]
                ),
            # These are libs that i want options for but meson has to handle
            ("partio", [True,False], True,[]),
            ("igl", [True,False], True,[]),
            ("eltopo", [True,False], True,[]),
            ]


__OPTIONS__ = {name:values for name,values,default,deps in __OPTIONAL_FLAGS_WITH_DEPS__}

__DEFAULT_OPTIONS__ = default_options = {name:default for name,values,default,deps in __OPTIONAL_FLAGS_WITH_DEPS__}


def dependencies():
    ret = set()
    for name, _, _, deps in __OPTIONAL_FLAGS_WITH_DEPS__:
        if getattr(__OPTIONS__,name):
            ret.update(deps)
    return ret


class Balsa(ConanFile):
    generators = "pkg_config"
    requires = __BASE_DEPS__ 
    settings = "os", "compiler", "build_type"  
    options = __OPTIONS__
    default_options = __DEFAULT_OPTIONS__


                                               
    def configure(self):
        for dep in dependencies():
            self.requires(dep)

        if self.options.visualization:
            self.options["glfw"].vulkan_static = True

    def build(self):                           
        meson = MesonToolchain(self)                   
        args = []
        for name, _, _, _ in __OPTIONAL_FLAGS_WITH_DEPS__:
            value = getattr(self.options,name)
            args.append("-D{}={}".format(name,  value))

                

        meson.configure(args=args)  
        meson.build()                          
                                               
                                               
