project(
  'substr8-hud',
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

gtk_dep = dependency('gtk4')
layer_shell_dep = dependency('gtk4-layer-shell-0', version: '>=1.0.0')
vte_dep = dependency('vte-2.91-gtk4')
gio_dep = dependency('gio-2.0')
gio_unix_dep = dependency('gio-unix-2.0')
wayfire_dep = dependency(
  'wayfire',
  version: ['>=0.10.0', '<0.11.0'],
)
wayfire_plugin_dir = wayfire_dep.get_variable(
  pkgconfig: 'plugindir',
)
wayfire_metadata_dir = wayfire_dep.get_variable(
  pkgconfig: 'metadatadir',
)

installed_module_dir = join_paths(
  get_option('prefix'),
  get_option('datadir'),
  'substr8-hud',
  'modules.d',
)

hud_sources = files(
  'src/main.c',
  'src/hud-app.c',
  'src/hud-config.c',
  'src/hud-config-write.c',
  'src/hud-state.c',
  'src/hud-surface.c',
  'src/hud-layout.c',
  'src/hud-module.c',
  'src/hud-settings.c',
  'src/hud-arrangement.c',
  'src/hud-dnd.c',
  'src/hud-navigation.c',
  'src/hud-opacity.c',
  'src/hud-output.c',
  'src/hud-slot.c',
  'src/hud-terminal.c',
  'src/hud-module-path.c',
  'src/hud-control.c',
)

executable(
  'wayfire-hud',
  hud_sources,
  c_args: [
    '-DSUBSTR8_HUD_DEFAULT_MODULE_DIR="' + installed_module_dir + '"',
  ],
  dependencies: [gtk_dep, layer_shell_dep, vte_dep, gio_dep, gio_unix_dep],
  install: true,
)

executable(
  'wayfire-hudctl',
  'src/hudctl.c',
  dependencies: [gio_dep, gio_unix_dep],
  install: true,
)

executable(
  'substr8-vt-mirror',
  'src/substr8-vt-mirror.c',
  install: true,
)

shared_module(
  'wayfire-hud',
  'src/wayfire-hud-plugin.cpp',
  dependencies: [wayfire_dep],
  install: true,
  install_dir: wayfire_plugin_dir,
)

install_data(
  'metadata/wayfire-hud.xml',
  install_dir: wayfire_metadata_dir,
)

install_data(
  'config/config.ini',
  install_dir: join_paths(get_option('datadir'), 'substr8-hud'),
)

install_data(
  'config/modules.d/00-tty.ini',
  'config/modules.d/00-wayfire-log.ini',
  'config/modules.d/01-htop.ini',
  'config/modules.d/02-btop.ini',
  'config/modules.d/05-nvtop.ini',
  'config/modules.d/10-shell.ini',
  'config/modules.d/20-opensnitchd.ini',
  'config/modules.d/21-nethogs.ini',
  'config/modules.d/30-iotop.ini',
  install_dir: installed_module_dir,
)

install_data(
  'config/modules.d/README.md',
  install_dir: join_paths(get_option('datadir'), 'substr8-hud'),
)

console_configuration = configuration_data()
console_configuration.set(
  'bindir',
  join_paths(get_option('prefix'), get_option('bindir')),
)
console_unit = configure_file(
  input: 'systemd/substr8-hud-console@.service.in',
  output: 'substr8-hud-console@.service',
  configuration: console_configuration,
)

install_data(
  'scripts/substr8-hud-console',
  install_dir: get_option('bindir'),
  install_mode: 'rwxr-xr-x',
)

install_data(
  'config/substr8-hud-console.tmux.conf',
  install_dir: join_paths(get_option('datadir'), 'substr8-hud'),
)

install_data(
  console_unit,
  install_dir: join_paths(
    get_option('prefix'),
    'lib',
    'systemd',
    'system',
  ),
)

btop_configuration = configuration_data()
btop_configuration.set(
  'libexecdir',
  join_paths(get_option('prefix'), get_option('libexecdir')),
)
btop_unit = configure_file(
  input: 'systemd/substr8-btop.service.in',
  output: 'substr8-btop.service',
  configuration: btop_configuration,
)

install_data(
  'scripts/substr8-btop',
  'scripts/substr8-btopctl',
  install_dir: get_option('bindir'),
  install_mode: 'rwxr-xr-x',
)

install_data(
  'scripts/substr8-btop-service',
  'scripts/substr8-btop-runner',
  install_dir: join_paths(get_option('libexecdir'), 'substr8-hud'),
  install_mode: 'rwxr-xr-x',
)

install_data(
  'config/btop/btop.conf',
  'config/btop/tmux.conf',
  'config/btop/environment',
  'config/btop/substr8-compact.theme',
  install_dir: join_paths(get_option('datadir'), 'substr8-hud', 'btop'),
)

install_data(
  btop_unit,
  install_dir: join_paths(
    get_option('prefix'),
    'lib',
    'systemd',
    'user',
  ),
)

install_data(
  'LICENSE',
  install_dir: join_paths(get_option('datadir'), 'licenses', 'fireshit'),
)
