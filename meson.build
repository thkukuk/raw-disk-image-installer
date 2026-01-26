project(
  'run0-wrappers',
  'c',
  meson_version : '>= 0.61.0',
  default_options : [
                  'prefix=/usr',
                  'sysconfdir=/etc',
                  'localstatedir=/var',
                  'buildtype=debugoptimized',
  		  'default_library=shared',
		  'b_pie=true',
                  'b_lto=true',
		  'warning_level=2'],
  license : ['MIT', 'GPL-2.0-or-later'],
  version : '0.1.0',
)

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE', meson.project_name())

cc = meson.get_compiler('c')
pkg = import('pkgconfig')

add_project_arguments(['-D_GNU_SOURCE=1',
		       '-DXTSTRINGDEFINES',
		       '-D_FORTIFY_SOURCE=2',
                       '-D_FILE_OFFSET_BITS=64',
                       '-D_TIME_BITS=64'], language : 'c')

possible_cc_flags = [
		  '-fstack-protector-strong',
		  '-funwind-tables',
		  '-fasynchronous-unwind-tables',
		  '-fstack-clash-protection',
		  '-Werror=return-type',
		  '-Wbad-function-cast',
		  '-Wcast-align',
                  '-Wformat-security',
		  '-Winline',
		  '-Wmissing-declarations',
		  '-Wmissing-prototypes',
		  '-Wnested-externs',
		  '-Wshadow',
		  '-Wstrict-prototypes',
		  '-Wundef',
		  ]
add_project_arguments(cc.get_supported_arguments(possible_cc_flags), language : 'c')

# installation directories
prefixdir = get_option('prefix')
if not prefixdir.startswith('/')
        error('Prefix is not absolute: "@0@"'.format(prefixdir))
endif
bindir = get_option('bindir')
datadir = get_option('datadir')
rdiidir = join_paths(prefixdir, get_option('libexecdir'), 'rdi-installer')
systemunitdir = join_paths(prefixdir, 'lib/systemd/system')

executable('keywait', 'src/keywait.c',
           install : true)

configure_file(
  input: 'scripts/rdi-installer.sh.in',
  output: 'rdi-installer',
  configuration: {
    'PACKAGE': meson.project_name(),
    'VERSION': meson.project_version(),
    'RDIIDIR': rdiidir,
  },
  install: true,
  install_dir: bindir
)

install_data('scripts/keymap.sh', install_dir : rdiidir)
install_data('scripts/select_image.sh', install_dir : rdiidir)
install_data('scripts/target_device.sh', install_dir : rdiidir)

configure_file(output: 'config.h', configuration: conf)

# Manual pages
subdir('man')
