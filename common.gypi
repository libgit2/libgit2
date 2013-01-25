{
  'target_defaults': {
    'default_configuration': 'Debug',
    'configurations': {
      'Debug': {
        'cflags': [ '-g', '-O0' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'AdditionalOptions': [
              '/Od',
              '/DEBUG',
              '/MTd',
              '/RTC1',
              '/RTCs',
              '/RTCu',
            ],
          },
        },
      },
      'Release': {
        'msvs_settings': {
          'VCCLCompilerTool': {
            'AdditionalOptions': [
              '/O2',
              '/MT',
            ],
          },
        },
      }
    }, # end of configurations
    'msvs_settings': {
      'VCCLCompilerTool': {
        'AdditionalOptions': [
          '/MP',
          '/nologo',
          '/Zi',
        ],
      }
    },
    'defines': [
      '_FILE_OFFSET_BITS=64'
    ],
    'cflags': [
      '-D_GNU_SOURCE',
      '-Wall',
      '-Wextra',
      '-Wno-missing-field-initializers',
      '-Wstrict-aliasing=2',
      '-Wstrict-prototypes'
    ],
    'conditions': [
      [ 'OS=="mac"', {
        'cflags': [
          '-Wno-deprecated-declarations',
        ]
      }],
    ]
  }
}
