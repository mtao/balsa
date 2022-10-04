from conans import ConanFile, tools, Meson
import os

class ConanFileToolsTest(ConanFile):
    generators = "pkg_config"
    requires = "shaderc/2021.1", "glfw/3.3.8"
    settings = "os", "compiler", "build_type"

    def build(self):
        meson = Meson(self)
        meson.configure(build_folder="build")
        meson.build()
