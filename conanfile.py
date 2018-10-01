from conans import ConanFile, CMake

class ThousandEyesFuturesConan(ConanFile):
    name = "thousandeyes-futures"
    version = "0.1"
    exports_sources = "include/*"
    no_copy_source = True

    def package(self):
        self.copy("*.h")

    def package_id(self):
        self.info.header_only()
