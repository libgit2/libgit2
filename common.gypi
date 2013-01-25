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
      },
      'VCLinkerTool': {
        'conditions': [
          ['target_arch=="x64"', {
            'TargetMachine' : 17 # /MACHINE:X64
          }],
        ]
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
      ['target_arch=="x64"', {
        'msvs_configuration_platform': 'x64',
      }],
      [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
        'conditions': [
          [ 'target_arch=="ia32"', {
            'cflags': [ '-m32' ],
            'ldflags': [ '-m32' ],
          }],
          [ 'target_arch=="x64"', {
            'cflags': [ '-m64' ],
            'ldflags': [ '-m64' ],
          }],
        ]
      }],
      [ 'OS=="mac"', {
        'cflags': [
          '-Wno-deprecated-declarations',
        ],
        'conditions': [
          ['target_arch=="ia32"', {
            'xcode_settings': {'ARCHS': ['i386']},
          }],
          ['target_arch=="x64"', {
            'xcode_settings': {'ARCHS': ['x86_64']},
          }],
        ]
      }],
    ]
  }
}
