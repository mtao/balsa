from conan import ConanFile
from conan.tools.gnu import PkgConfigDeps
from conan.tools.meson import MesonToolchain


class Balsa(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "PkgConfigDeps", "VirtualRunEnv"

    requires = [
        "eigen/3.4.0",
        "spdlog/1.15.3",
        "nlohmann_json/3.12.0",
        "catch2/3.8.1",
        "cli11/2.5.0",
        "onetbb/2022.0.0",
    ]

    def configure(self):
        self.options["hwloc"].shared = True

    def generate(self):
        meson = MesonToolchain(self)
        meson.generate()
