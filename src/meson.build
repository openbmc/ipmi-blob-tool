ipmiblob_incs = include_directories('.')

install_headers(
  'crc.hpp',
  'blob_errors.hpp',
  'blob_interface.hpp',
  'blob_handler.hpp',
  'ipmi_interface.hpp',
  'ipmi_handler.hpp',
  subdir: 'ipmiblob')

install_headers(
  'internal/sys.hpp',
  'internal/sys_interface.hpp',
 subdir: 'ipmiblob/internal')

install_headers(
  'test/blob_interface_mock.hpp',
  'test/crc_mock.hpp',
  'test/ipmi_interface_mock.hpp',
 subdir: 'ipmiblob/test')

ipmiblob_lib = library(
  'ipmiblob',
  'blob_handler.cpp',
  'crc.cpp',
  'ipmi_handler.cpp',
  'internal/sys.cpp',
  include_directories : ipmiblob_incs,
  implicit_include_directories: false,
  version: meson.project_version(),
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
