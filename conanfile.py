from conans import ConanFile, CMake

class ThousandEyesFuturesConan(ConanFile):
    name = "thousandeyes-futures"
    version = "0.5"
    exports_sources = "include/*", "FindThousandEyesFutures.cmake"
    no_copy_source = True
    short_paths = True

    def package(self):
        self.copy("*.h")
        self.copy("FindThousandEyesFutures.cmake", dst=".", src=".")

    def package_id(self):
        self.info.header_only()
