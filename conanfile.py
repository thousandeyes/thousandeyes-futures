from conans import ConanFile, CMake

class ThousandEyesFuturesConan(ConanFile):
    name = "thousandeyes-futures"
    version = "0.1"
    exports_sources = "include/*", "FindThousandEyesFutures.cmake"
    no_copy_source = True

    def package(self):
        self.copy("*.h")
        self.copy("FindThousandeyesFutures.cmake", dst=".", src=".")

    def package_id(self):
        self.info.header_only()
