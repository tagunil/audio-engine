import qbs

Project {
    minimumQbsVersion: "1.7"

    CppApplication {
        consoleApplication: true

        cpp.warningLevel: "all"
        cpp.treatWarningsAsErrors: true

        cpp.cxxLanguageVersion: "c++17"

        cpp.defines: [
            "_REENTRANT",
            "HAS_IEEE_FLOAT",
            "HAS_COSINE_TABLE"
        ]

        cpp.dynamicLibraries: [
            "pulse-simple",
            "pulse"
        ]

        cpp.includePaths: [
            "src"
        ]

        files: [
            "cli/audiotrack.cpp",
            "src/audiotrack.cpp",
            "src/audiotrack.h",
            "src/wavreader.cpp",
            "src/wavreader.h",
            "src/audioreader.h",
            "src/cosine.cpp",
            "src/cosine.h",
        ]

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }
}
