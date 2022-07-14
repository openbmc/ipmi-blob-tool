project(
  'ipmiblob',
  'cpp',
  version: '0.1',
  meson_version: '>=0.61.3',
  default_options: [
    'cpp_std=c++20',
    'werror=true',
    'warning_level=3',
    'tests=' + (meson.is_subproject() ? 'disabled' : 'auto'),
  ])

subdir('src')
if not get_option('tests').disabled()
  subdir('test')
endif
