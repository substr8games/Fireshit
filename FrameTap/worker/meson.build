wayland_client_dep = dependency('wayland-client', required: true)
wayland_protocols_dep = dependency('wayland-protocols', required: true)
wayland_scanner = find_program('wayland-scanner', required: true)
protocol_data_dir = wayland_protocols_dep.get_variable('pkgdatadir')

libavcodec_dep = dependency('libavcodec', required: true)
libavformat_dep = dependency('libavformat', required: true)
libavutil_dep = dependency('libavutil', required: true)
libavfilter_dep = dependency('libavfilter', required: true)
libva_dep = dependency('libva', required: true)
libdrm_dep = dependency('libdrm', required: true)

xdg_shell_xml = join_paths(protocol_data_dir, 'stable', 'xdg-shell', 'xdg-shell.xml')
linux_dmabuf_xml = join_paths(protocol_data_dir, 'unstable', 'linux-dmabuf', 'linux-dmabuf-unstable-v1.xml')

xdg_shell_header = custom_target(
  'xdg-shell-client-header',
  input: xdg_shell_xml,
  output: 'xdg-shell-client-protocol.h',
  command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)
xdg_shell_code = custom_target(
  'xdg-shell-client-code',
  input: xdg_shell_xml,
  output: 'xdg-shell-client-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)
linux_dmabuf_header = custom_target(
  'linux-dmabuf-client-header',
  input: linux_dmabuf_xml,
  output: 'linux-dmabuf-unstable-v1-client-protocol.h',
  command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)
linux_dmabuf_code = custom_target(
  'linux-dmabuf-client-code',
  input: linux_dmabuf_xml,
  output: 'linux-dmabuf-unstable-v1-client-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

frametap_worker_args = ['-DFRAMETAP_VERSION="@0@"'.format(meson.project_version())]
frametap_worker_deps = [
  frametap_protocol_dep,
  threads_dep,
  wayland_client_dep,
  libavcodec_dep,
  libavformat_dep,
  libavutil_dep,
  libavfilter_dep,
  libva_dep,
  libdrm_dep,
]
frametap_worker_sources = files(
  'src/main.cpp',
  'src/cli.cpp',
  'src/help.cpp',
  'src/config.cpp',
  'src/config_files.cpp',
  'src/launch.cpp',
  'src/application_audio.cpp',
  'src/audio_finalize.cpp',
  'src/command_process.cpp',
  'src/plugin_client.cpp',
  'src/session.cpp',
  'src/target_request.cpp',
  'src/region_selector.cpp',
  'src/region_selector_draw.cpp',
  'src/region_selector_events.cpp',
  'src/loop_output.cpp',
  'src/vaapi_encoder.cpp',
  'src/vaapi_import.cpp',
  'src/vaapi_mux.cpp',
  'src/buffer_pool.cpp',
) + [xdg_shell_header, xdg_shell_code, linux_dmabuf_header, linux_dmabuf_code]
