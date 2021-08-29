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
            "HAS_IEEE_FLOAT"
        ]

        cpp.dynamicLibraries: [
            "pulse-simple",
            "pulse"
        ]

        cpp.includePaths: [
            "src"
        ]

        Group {
            name: "Project sources"

            files: [
                "cli/wavreader.cpp",
                "src/wavreader.cpp",
                "src/wavreader.h",
                "src/audioreader.h",
            ]
        }

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }
}
