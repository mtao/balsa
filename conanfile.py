from conans import ConanFile, tools, Meson
import os

class Balsa(ConanFile):
    generators = "pkg_config"
    requires = "spdlog/1.10.0", "range-v3/0.12.0",  "fmt/9.1.0"
    settings = "os", "compiler", "build_type"  
    options = {
            "visualization": [True,False],
            "openvdb": [True,False],
            "protobuf": [True,False],
            }
    default_options = {
            "visualization": True,
            "openvdb": True,
            "protobuf": True,
            }
                                               
    def configure(self):
        if self.options.visualization:
            self.requires("shaderc/2021.1", "glfw/3.3.8", "glm/0.9.9.8")
        if self.options.openvdb:
            self.requires("openvdb/8.0.1")
        if self.options.protobuf:
            self.requires("protobuf/3.11.4")
    def build(self):                           
        meson = Meson(self)                   
        args = []
        if self.options.visualization:
            args.append("-Dvisualization=true")
        if self.options.openvdb:
            args.append("-Dopenvdb=true")
        if self.options.protobuf:
            args.append("-Dprotobuf=true")

        meson.configure(build_folder="build", args=args)  
        meson.build()                          
                                               
                                               
