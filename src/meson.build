ipmiblob_incs = include_directories('.')

install_headers(
  'ipmiblob/crc.hpp',
  'ipmiblob/blob_errors.hpp',
  'ipmiblob/blob_interface.hpp',
  'ipmiblob/blob_handler.hpp',
  'ipmiblob/ipmi_interface.hpp',
  'ipmiblob/ipmi_handler.hpp',
  subdir: 'ipmiblob')

install_headers(
  'ipmiblob/internal/sys.hpp',
  'ipmiblob/internal/sys_interface.hpp',
 subdir: 'ipmiblob/internal')

install_headers(
  'ipmiblob/test/blob_interface_mock.hpp',
  'ipmiblob/test/crc_mock.hpp',
  'ipmiblob/test/ipmi_interface_mock.hpp',
 subdir: 'ipmiblob/test')

ipmiblob_lib = library(
  'ipmiblob',
  'ipmiblob/blob_handler.cpp',
  'ipmiblob/crc.cpp',
  'ipmiblob/ipmi_handler.cpp',
  'ipmiblob/internal/sys.cpp',
  include_directories : ipmiblob_incs,
  implicit_include_directories: false,
  version: meson.project_version(),
  dependencies: [dependency('stdplus')],
  install: true,
  install_dir: get_option('libdir')
)

ipmiblob = declare_dependency(
  include_directories: ipmiblob_incs,
  link_with: ipmiblob_lib
)

import('pkgconfig').generate(
  ipmiblob_lib,
  name: 'ipmiblob',
  version: meson.project_version(),
  description: 'ipmiblob')
