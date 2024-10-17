from conans import ConanFile, tools, Meson
import os

__BASE_DEPS__ = [
        "spdlog/1.10.0", 
        "eigen/3.4.0",
        "range-v3/0.12.0",  
        "fmt/9.1.0", 
        "onetbb/2020.3",  # openvdb 8.1.0 requires this 
        "catch2/3.1.0",
        "cxxopts/3.0.0",
        ]

__OPTIONAL_FLAGS_WITH_DEPS__ = [
            ("visualization", [True,False], True, [
                "shaderc/2021.1", "glfw/3.3.8", "glm/0.9.9.8", "imgui/1.88"
                ]),
            ("openvdb", [True,False], True,
                ["openvdb/8.0.1", "c-blosc/1.21.3", "zlib/1.2.13"]
                ),
            ("protobuf", [True,False], True,
                ["protobuf/3.11.4"]
                ),
            ("json", [True,False], True,
                ["nlohmann_json/3.11.2"]
                ),
            ("embree", [True,False], True,
                ["embree3/3.13.3"]
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




class Balsa(ConanFile):
    generators = "pkg_config"
    requires = __BASE_DEPS__ 
    settings = "os", "compiler", "build_type"  
    options = __OPTIONS__
    default_options = __DEFAULT_OPTIONS__
                                               
    def configure(self):
        for name, _, _, deps in __OPTIONAL_FLAGS_WITH_DEPS__:
            if getattr(self.options,name):
                for dep in deps:
                    self.requires(dep)

        if self.options.visualization:
            self.options["glfw"].vulkan_static = True

    def build(self):                           
        meson = Meson(self)                   
        args = []
        for name, _, _, _ in __OPTIONAL_FLAGS_WITH_DEPS__:
            value = getattr(self.options,name)
            args.append("-D{}={}".format(name,  value))

                

        meson.configure(args=args)  
        meson.build()                          
                                               
                                               
