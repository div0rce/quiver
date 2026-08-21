"""Conan recipe skeleton for Quiver (REQ-BUILD-015).

Prepared at M8; ConanCenter submission is a release activity, not a build-system feature. The
recipe consumes the standard CMake install/export (REQ-BUILD-009) and exposes the
``quiver::quiver`` target via ``find_package(Quiver)``. Zero runtime dependencies (Charter T4).
``version`` tracks the code version single-sourced in include/quiver/detail/config.h.
"""

import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy


class QuiverConan(ConanFile):
    name = "quiver"
    version = "0.8.0"
    license = "Apache-2.0"
    homepage = "https://github.com/div0rce/quiver"
    url = "https://github.com/div0rce/quiver"
    description = (
        "Dependency-free C++23 vectorized analytical kernels + cross-ISA performance ledger"
    )
    topics = ("simd", "analytics", "kernels", "cpp23")
    settings = "os", "arch", "compiler", "build_type"
    package_type = "static-library"
    exports_sources = "CMakeLists.txt", "CMakePresets.json", "include/*", "src/*", "cmake/*"

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["QUIVER_ENABLE_TESTS"] = "OFF"
        tc.cache_variables["QUIVER_ENABLE_EXAMPLES"] = "OFF"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "Quiver")
        self.cpp_info.set_property("cmake_target_name", "quiver::quiver")
        self.cpp_info.libs = ["quiver"]
