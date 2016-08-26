{
  'targets': [
    {
      'target_name': 'http_parser',
      'type': 'static_library',
      'include_dirs': [ '.' ],
      'direct_dependent_settings': {
        'include_dirs': [ '.' ],
      },
      'sources': [ './http_parser.c', ]
    },
  ]
}
