{
  "targets": [
    {
      "target_name": "html",
      "sources": [
        "src/addon.cc",
        "src/html_document.cc",
        "src/html_element.cc",
        "src/xnode.c",
        "src/xnode_query.c",
        "src/xnode_query_parser.c",
        "src/jsa.c",
        "src/gumbo-parser/src/attribute.c",
        "src/gumbo-parser/src/error.c",
        "src/gumbo-parser/src/string_buffer.c",
        "src/gumbo-parser/src/tag.c",
        "src/gumbo-parser/src/utf8.c",
        "src/gumbo-parser/src/vector.c",
        "src/gumbo-parser/src/char_ref.c",
        "src/gumbo-parser/src/parser.c",
        "src/gumbo-parser/src/string_piece.c",
        "src/gumbo-parser/src/tokenizer.c",
        "src/gumbo-parser/src/util.c"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "src",
        "src/gumbo-parser/src"
      ],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "xcode_settings": {
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.15"
      },
      "msvs_settings": {
        "VCCLCompilerTool": {
          "ExceptionHandling": 1
        }
      },
      "conditions": [
        ["OS==\"win\"", {
          "include_dirs": ["src/include/win"]
        }]
      ]
    }
  ]
}
