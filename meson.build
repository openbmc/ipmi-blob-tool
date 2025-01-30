project(
  'ipmiblob',
  'cpp',
  version: '0.1',
  meson_version: '>=1.1.1',
  default_options: [
    'cpp_std=c++23',
    'werror=true',
    'warning_level=3',
    'tests=' + (meson.is_subproject() ? 'disabled' : 'auto'),
  ])

subdir('src')
if get_option('tests').allowed()
  subdir('test')
endif
