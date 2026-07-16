project(
  'frametap',
  ['cpp', 'c'],
  version: '1.1.0',
  license: 'LicenseRef-FCIL-0.1',
  meson_version: '>=0.63.0',
  default_options: [
    'cpp_std=c++20',
    'buildtype=release',
    'warning_level=3',
    'werror=false',
    'b_lto=true',
  ],
)

cpp = meson.get_compiler('cpp')
threads_dep = dependency('threads')
inc = include_directories('protocol/include', 'plugin/include', 'worker/include')

add_project_arguments('-D_FILE_OFFSET_BITS=64', language: ['cpp', 'c'])

subdir('protocol')

if get_option('worker')
  subdir('worker')
  frametap_executable = executable(
    'frametap',
    frametap_worker_sources,
    include_directories: inc,
    dependencies: frametap_worker_deps,
    cpp_args: frametap_worker_args,
    install: true,
  )
endif

wayfire_dep = dependency('wayfire', version: ['>=0.10.0', '<0.11.0'], required: get_option('plugin'))
wlroots_dep = dependency('wlroots-0.19', required: get_option('plugin'))
if wayfire_dep.found() and wlroots_dep.found()
  subdir('plugin')
  frametap_wayfire_plugin = shared_module(
    'frametap-wayfire',
    frametap_plugin_sources,
    include_directories: inc,
    dependencies: frametap_plugin_deps,
    cpp_args: ['-DWAYFIRE_PLUGIN', '-DWLR_USE_UNSTABLE'],
    name_prefix: 'lib',
    install: true,
    install_dir: plugin_dir,
  )
elif get_option('plugin').enabled()
  error('FrameTap requires compatible Wayfire and wlroots 0.19 development files')
else
  message('Wayfire plugin explicitly disabled')
endif

if get_option('tests').enabled()
  subdir('tests')
endif

install_data('LICENSE', install_dir: join_paths(get_option('datadir'), 'licenses', 'fireshit'))
install_data('config/frametap.ini.example',
  install_dir: join_paths(get_option('datadir'), 'doc', 'frametap'))

summary({
  'canonical executable': get_option('worker'),
  'Wayfire plugin': wayfire_dep.found(),
  'developer tests': get_option('tests'),
  'protocol version': 5,
  'application audio': 'private PipeWire-Pulse sink + Opus',
  'output audio': 'default sink monitor + Opus',
  'spatial capture': 'whole output + one-output region',
  'user config': '$XDG_CONFIG_HOME/frametap/config.ini',
}, section: 'FrameTap')
