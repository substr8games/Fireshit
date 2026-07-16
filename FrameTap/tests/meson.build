protocol_test = executable(
  'frametap-protocol-test',
  'protocol_test.cpp',
  include_directories: inc,
  dependencies: frametap_protocol_dep,
)
test('protocol round trip', protocol_test)

seqpacket_test = executable(
  'frametap-seqpacket-test',
  'seqpacket_test.cpp',
  include_directories: inc,
  dependencies: frametap_protocol_dep,
)
test('seqpacket FD passing', seqpacket_test)

cli_test = executable(
  'frametap-cli-test',
  ['cli_test.cpp', '../worker/src/cli.cpp', '../worker/src/config.cpp', '../worker/src/config_files.cpp'],
  include_directories: inc,
  dependencies: frametap_protocol_dep,
)
test('CLI parsing', cli_test)

config_test = executable(
  'frametap-config-test',
  ['config_test.cpp', '../worker/src/config.cpp', '../worker/src/config_files.cpp'],
  include_directories: inc,
)
test('user config', config_test)
