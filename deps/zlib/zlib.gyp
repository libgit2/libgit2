{
  'targets': [
    {
      'target_name': 'zlib',
      'type': 'static_library',
      'sources': [
        'adler32.c',
        'crc32.c',
        'crc32.h',
        'deflate.c',
        'deflate.h',
        'inffast.c',
        'inffast.h',
        'inffixed.h',
        'inflate.c',
        'inflate.h',
        'inftrees.c',
        'inftrees.h',
        'trees.c',
        'trees.h',
        'zconf.h',
        'zlib.h',
        'zutil.c',
        'zutil.h',
      ],
      'include_dirs': [
        '.',
        '../../include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
    },
  ]
}
