{
  'targets': [
    {
      'target_name': 'liburing',
      'type': 'static_library',
      'sources': [
        'src/queue.c',
        'src/register.c',
        'src/setup.c',
        'src/syscall.c'
      ],
      'cflags!': [
        '-ansi'
      ],
      'include_dirs': [
        'src/include'
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src/include'
        ]
      },
      'defines': [
      ],
      'libraries': [
        '-luring'
      ]
    }
  ]
}
