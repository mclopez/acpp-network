from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy

class LibacppNetworkConan(ConanFile):
    name = "acpp-network"
    version = "0.1.0"
    
    # Optional metadata
    license = "MIT"
    author = "Your Name"
    url = "https://github.com/yourusername/libacpp-network"
    description = "A C++ Network library"
    
    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False], "log_level": ["0", "1"], "acpp_bio": [True, False]}
    default_options = {"shared": False, "fPIC": True, "log_level": "0", "acpp_bio": True }
    
    # Add generators
    generators = "CMakeDeps" #, "CMakeToolchain"
    
    # Sources are located in the same place as this recipe
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "test/*"

    def requirements(self):
#        self.test_requires("gtest/1.14.0")
        self.requires("spdlog/1.12.0", transitive_headers=True)
        #self.requires("openssl/3.6.0", transitive_headers=True)
        self.requires("openssl/3.1.4", transitive_headers=True)
        

    def build_requirements(self):
        self.test_requires("gtest/1.14.0")



    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def generate(self):
        tc = CMakeToolchain(self)
        print("debug options:", {k: v for k, v in self.options.items()})
        if self.options and self.options.log_level:
            tc.preprocessor_definitions["LOG_LEVEL"] = self.options.log_level
        if self.options and self.options.acpp_bio == True:
            print("debug acpp_bio option: on")
            tc.preprocessor_definitions["ACPP_BIO"] = "1"
        else:
            print("debug acpp_bio option: off")

        tc.generate()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.defines.append(f"LOG_LEVEL={str(self.options.log_level)}")
        if self.options.acpp_bio and self.options.acpp_bio == True:
            self.cpp_info.defines.append(f"ACPP_BIO={str(self.options.acpp_bio)}")
        self.cpp_info.libs = ["acpp-network"]
        self.cpp_info.set_property("cmake_file_name", "acpp-network")
        self.cpp_info.set_property("cmake_target_name", "acpp-network::acpp-network")
        self.cpp_info.includedirs = ["include"]
        self.cpp_info.requires = ["spdlog::spdlog", "openssl::openssl"]
        if self.options.shared:
            self.cpp_info.defines.append("ACPPJSON_SHARED")