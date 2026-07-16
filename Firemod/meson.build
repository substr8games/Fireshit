project(
  'firemod',
  'c',
  'cpp',
  version: '1.1.0',
  license: 'LicenseRef-FCIL-0.1',
  default_options: [
    'c_std=c17',
    'cpp_std=c++17',
    'warning_level=2',
  ],
)

gnome = import('gnome')
cc = meson.get_compiler('c')

gtk_dep = dependency('gtk4', version: '>=4.10.0')
layer_shell_dep = dependency('gtk4-layer-shell-0', version: '>=1.0.0')
gio_dep = dependency('gio-2.0')
gio_unix_dep = dependency('gio-unix-2.0')
glib_dep = dependency('glib-2.0')
wayland_client_dep = dependency('wayland-client')
wlr_protocols_dep = dependency('wlr-protocols')
math_dep = cc.find_library('m', required: true)
wayfire_dep = dependency('wayfire', version: ['>=0.10.0', '<0.11.0'])

module_dir = join_paths(get_option('prefix'), get_option('datadir'),
  'firemod', 'modules.d')
capability_dir = join_paths(get_option('prefix'), get_option('datadir'),
  'firemod', 'capabilities.d')
wayfire_plugin_dir = wayfire_dep.get_variable(pkgconfig: 'plugindir')
wayfire_metadata_dir = wayfire_dep.get_variable(pkgconfig: 'metadatadir')

configuration = configuration_data()
configuration.set('FIREMOD_MODULE_DIR', module_dir)
configuration.set('FIREMOD_CAPABILITY_DIR', capability_dir)
configuration.set('FIREMOD_VERSION', meson.project_version())
configure_file(
  input: 'src/firemod-build-config.h.in',
  output: 'firemod-build-config.h',
  configuration: configuration,
)

wayland_scanner = find_program('wayland-scanner')
wlr_protocol_dir = wlr_protocols_dep.get_variable(pkgconfig: 'pkgdatadir')
screencopy_xml = join_paths(wlr_protocol_dir,
  'unstable', 'wlr-screencopy-unstable-v1.xml')
screencopy_header = custom_target(
  'wlr-screencopy-client-header',
  input: screencopy_xml,
  output: 'wlr-screencopy-unstable-v1-client-protocol.h',
  command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)
screencopy_code = custom_target(
  'wlr-screencopy-client-code',
  input: screencopy_xml,
  output: 'wlr-screencopy-unstable-v1-client-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

resources = gnome.compile_resources(
  'firemod-resources',
  'data/firemod.gresource.xml',
  source_dir: 'data',
  c_name: 'firemod_resources',
)

sources = files(
  'src/main.c',
  'src/firemod-actions.c',
  'src/firemod-app-index.c',
  'src/firemod-action-editor.c',
  'src/firemod-app.c',
  'src/firemod-authority-widget.c',
  'src/firemod-capability.c',
  'src/firemod-capture.c',
  'src/firemod-capture-select.c',
  'src/firemod-color.c',
  'src/firemod-color-picker.c',
  'src/firemod-discovery.c',
  'src/firemod-dispatch.c',
  'src/firemod-first-party.c',
  'src/firemod-fireshit-page.c',
  'src/firemod-hex-assets.c',
  'src/firemod-hex-layout.c',
  'src/firemod-hex-preview.c',
  'src/firemod-menu.c',
  'src/firemod-model.c',
  'src/firemod-pages.c',
  'src/firemod-parse.c',
  'src/firemod-paths.c',
  'src/firemod-picker-result.c',
  'src/firemod-picker-view.c',
  'src/firemod-registry.c',
  'src/firemod-runtime.c',
  'src/firemod-screencopy.c',
  'src/firemod-screencopy-frame.c',
  'src/firemod-settings-model.c',
  'src/firemod-settings-io.c',
  'src/firemod-settings-save.c',
  'src/firemod-settings-apply.c',
  'src/firemod-shell.c',
  'src/firemod-widgets.c',
  'src/firemod-xdg.c',
)

executable(
  'firemod',
  sources,
  screencopy_header,
  screencopy_code,
  resources,
  include_directories: include_directories('.', 'src'),
  dependencies: [
    gtk_dep,
    layer_shell_dep,
    gio_dep,
    gio_unix_dep,
    wayland_client_dep,
    math_dep,
  ],
  install: true,
)

executable(
  'firemodctl',
  'src/firemodctl.c',
  dependencies: [gio_dep, gio_unix_dep],
  install: true,
)

shared_module(
  'firemod-wayfire',
  'src/wayfire/firemod-wayfire-plugin.cpp',
  dependencies: [wayfire_dep],
  install: true,
  install_dir: wayfire_plugin_dir,
)

color_test = executable(
  'firemod-color-test',
  'tests/test-color.c',
  'src/firemod-color.c',
  include_directories: include_directories('src'),
  dependencies: [glib_dep, math_dep],
)
test('color-conversion', color_test)

install_data(
  'data/org.philabs.Firemod.desktop',
  install_dir: join_paths(get_option('datadir'), 'applications'),
)
install_data(
  'data/org.philabs.Firemod.metainfo.xml',
  install_dir: join_paths(get_option('datadir'), 'metainfo'),
)
install_data(
  'data/wayfire/firemod.xml',
  install_dir: wayfire_metadata_dir,
)
install_subdir(
  'data/modules.d',
  install_dir: join_paths(get_option('datadir'), 'firemod'),
)
install_subdir(
  'data/capabilities.d',
  install_dir: join_paths(get_option('datadir'), 'firemod'),
)

install_data(
  'LICENSE',
  install_dir: join_paths(get_option('datadir'), 'licenses', 'fireshit'),
)
