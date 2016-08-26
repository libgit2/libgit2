{
  'targets': [
    {
      'target_name': 'gnu_regex',
      'type': 'static_library',
      'sources': [
        'regex.c',
      ],
      'include_dirs': [
        '.',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
    },
  ]
}

