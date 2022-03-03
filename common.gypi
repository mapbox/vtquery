{
  'target_defaults': {
    'default_configuration': 'Release',
    'cflags_cc' : [ '-std=c++14' ],
    'cflags_cc!': ['-std=gnu++0x','-std=gnu++1y', '-fno-rtti', '-fno-exceptions'],
    'configurations': {
      'Debug': {
        'defines!': [
          'NDEBUG'
        ],
        'cflags_cc!': [
          '-O3',
          '-Os',
          '-DNDEBUG'
        ],
        'xcode_settings': {
          'OTHER_CPLUSPLUSFLAGS!': [
            '-O3',
            '-Os',
            '-DDEBUG'
          ],
          'GCC_OPTIMIZATION_LEVEL': '0',
          'GCC_GENERATE_DEBUGGING_SYMBOLS': 'YES'
        }
      },
      'Release': {
        'defines': [
          'NDEBUG'
        ],
        'cflags': [
         '-flto'
        ],
        'ldflags': [
         '-flto',
         '-fuse-ld=<(module_root_dir)/mason_packages/.link/bin/ld'
        ],
        'xcode_settings': {
          'OTHER_CPLUSPLUSFLAGS!': [
            '-Os',
            '-O2'
          ],
          'OTHER_LDFLAGS':[ '-flto' ],
          'OTHER_CPLUSPLUSFLAGS': [ '-flto' ],
          'GCC_OPTIMIZATION_LEVEL': '3',
          'GCC_GENERATE_DEBUGGING_SYMBOLS': 'NO',
          'DEAD_CODE_STRIPPING': 'YES',
          'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES'
        }
      }
    }
  }
}
