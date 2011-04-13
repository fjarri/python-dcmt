from distutils.core import setup, Extension

libdcmt = Extension('dcmt._libdcmt',
	sources = [
		'src/dcmt/lib/check32.c',
		'src/dcmt/lib/eqdeg.c',
		'src/dcmt/lib/genmtrand.c',
		'src/dcmt/lib/init.c',
		'src/dcmt/lib/mt19937.c',
		'src/dcmt/lib/prescr.c',
		'src/dcmt/lib/seive.c',

		'src/wrapper/wrapper.c'
	],
	include_dirs = ['src/dcmt/include', 'src/dcmt/lib'],
	extra_compile_args = ['-Wall', '-Wmissing-prototypes', '-O3', '-std=c99'])

setup(name = 'dcmt',
	packages = ['dcmt'],
	version = '0.6.1',
	description = 'Dynamic creation of Mersenne twister RNGs',
	ext_modules = [libdcmt])
