wayland_server_dep = dependency('wayland-server')
plugin_dir = get_option('plugin_dir')
if plugin_dir == ''
  plugin_dir = wayfire_dep.get_variable(pkgconfig: 'plugindir', default_value: join_paths(get_option('libdir'), 'wayfire'))
endif
metadata_dir = get_option('metadata_dir')
if metadata_dir == ''
  metadata_dir = wayfire_dep.get_variable(pkgconfig: 'metadatadir', default_value: join_paths(get_option('datadir'), 'wayfire', 'metadata'))
endif

frametap_plugin_sources = files(
  'src/plugin.cpp',
  'src/server.cpp',
  'src/server_catalog.cpp',
  'src/capture_backend.cpp',
  'src/capture_source.cpp',
  'src/linear_buffer_pool.cpp',
)

frametap_plugin_deps = [
  wayfire_dep,
  wlroots_dep,
  wayland_server_dep,
  frametap_protocol_dep,
  threads_dep,
]

install_data('metadata/frametap.xml', install_dir: metadata_dir)
