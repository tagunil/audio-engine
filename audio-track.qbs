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
            "mp3dec/inc",
            "src"
        ]

        Group {
            name: "Project sources"

            files: [
                "cli/audiotrack.cpp",
                "src/audiotrack.cpp",
                "src/audiotrack.h",
                "src/mp3reader.cpp",
                "src/mp3reader.h",
                "src/wavreader.cpp",
                "src/wavreader.h",
                "src/audioreader.h",
                "src/cosine.cpp",
                "src/cosine.h",
            ]
        }

        Group {
            name: "Helix sources"

            cpp.commonCompilerFlags: [
                "-Wno-unused-but-set-variable",
                "-Wno-unused-parameter"
            ]

            files: [
                "mp3dec/inc/mp3dec.h",
                "mp3dec/inc/mp3common.h",
                "mp3dec/inc/statname.h",
                "mp3dec/src/mp3dec.c",
                "mp3dec/src/mp3tabs.c",
                "mp3dec/src/assembly.h",
                "mp3dec/src/bitstream.c",
                "mp3dec/src/coder.h",
                "mp3dec/src/dct32.c",
                "mp3dec/src/dequant.c",
                "mp3dec/src/dqchan.c",
                "mp3dec/src/huffman.c",
                "mp3dec/src/hufftabs.c",
                "mp3dec/src/imdct.c",
                "mp3dec/src/polyphase.c",
                "mp3dec/src/scalfact.c",
                "mp3dec/src/stproc.c",
                "mp3dec/src/subband.c",
                "mp3dec/src/trigtabs.c",
            ]
        }

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }
}
