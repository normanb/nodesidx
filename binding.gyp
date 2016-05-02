{
  'targets': [
    {
      'target_name': 'spatialindex',
      'product_extension': 'node',
      'type': 'shared_library',
      'conditions': [
        [
          'OS=="linux"', {
            'cflags_cc!' : [
              '-std=gnu++0x'
            ],
            'cflags_cc' : [
              '-std=c++11'
            ]
          }
        ],
        [
          'OS=="mac"', {
            # node-gyp 2.x doesn't add this anymore
            # https://github.com/TooTallNate/node-gyp/pull/612
            'xcode_settings': {
              'CLANG_CXX_LIBRARY': 'libc++',
              'CLANG_CXX_LANGUAGE_STANDARD':'c++11',
              'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
              'MACOSX_DEPLOYMENT_TARGET':'10.8',
              'OTHER_LDFLAGS':[
                '-Wl,-bind_at_load',
                '-undefined dynamic_lookup'
              ]
            }
          }
        ]
      ],
      'include_dirs': [
        "<!(node -e \"require('nan')\")",
        'deps/libspatialindex/include'
      ],
      'sources': [
        'src/spatialindex.cc',
        'src/libsidxjs.cc'
      ],
      "libraries": [ "-Wl,-rpath,<!(pwd)/build/Release/"],
      'dependencies': [
        'deps/libspatialindex/libspatialindex.gyp:libspatialindex'
      ]
    }
  ]
}
