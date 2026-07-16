protocol_sources = files(
  'src/codec.cpp',
  'src/codec_catalog.cpp',
  'src/seqpacket.cpp',
)

frametap_protocol = static_library(
  'frametap-protocol',
  protocol_sources,
  include_directories: inc,
  dependencies: threads_dep,
  pic: true,
)

frametap_protocol_dep = declare_dependency(
  link_with: frametap_protocol,
  include_directories: inc,
  dependencies: threads_dep,
)
