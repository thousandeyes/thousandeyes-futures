import com.thousandeyes.dsl.ConanJob
import com.thousandeyes.dsl.OsType

def job = new ConanJob(this)
job.config {
    displayName 'LIB thousandeyes-futures'
    githubProject 'conan-thousandeyes-futures'

    buildConfigs {
        osType OsType.MACOS
        label 'eyebrow-build-mac'
        pythonEnvPath '/te/opt/conan-virtualenv/bin/activate'

        packageConfig {
            profile 'macos-clang70-x64-release'
            profile 'macos-clang70-x64-debug'
        }
    }

    buildConfigs {
        osType OsType.WINDOWS
        label 'eyebrow-build-win64'

        packageConfig {
            profile 'windows-msvc15-x86-release'
            profile 'windows-msvc15-x64-release'
        }
    }
}

