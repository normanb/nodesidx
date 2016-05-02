{
  'targets': [
    {
      'target_name': 'libspatialindex',
      'type': 'shared_library',
      'sources': ['<!@(find ./src -name "*.cc")'],
      'cflags!': [ '-fno-exceptions', '-fno-rtti'],
      'cflags_cc!': [ '-fno-exceptions', '-fno-rtti'],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
            'GCC_ENABLE_CPP_RTTI': 'YES'
          }
        }]
      ],
      'include_dirs': [
        'include/',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
          'include/capi'
        ],
      },
    }
  ]
}
