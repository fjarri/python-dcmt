try:
    from setuptools import setup, Extension
except ImportError:
    from distutils.core import setup, Extension

import sys
major, minor, _, _, _ = sys.version_info
if not (major == 2 and minor >= 5):
	print("Python >=2.5 is required to use this module.")
	sys.exit(1)

import os.path

def get_symbol(path, symbol):
	full_path = os.path.join(setup_dir, *path)
	globals_dict = {}
	execfile(full_path, globals_dict)
	return globals_dict[symbol]


setup_dir = os.path.split(os.path.abspath(__file__))[0]
DOCUMENTATION = open(os.path.join(setup_dir, 'README.rst')).read()

VERSION = get_symbol(('dcmt', 'version.py'), 'VERSION_STRING')


class NumpyExtension(Extension):
	# nicked from
	# http://mail.python.org/pipermail/distutils-sig/2007-September/008253.html
	# solution by Michael Hoffmann
	def __init__(self, *args, **kwargs):
		Extension.__init__(self, *args, **kwargs)
		self._include_dirs = self.include_dirs
		del self.include_dirs # restore overwritten property

	def get_numpy_incpath(self):
		from imp import find_module
		# avoid actually importing numpy, it screws up distutils
		file, pathname, descr = find_module("numpy")
		from os.path import join
		return join(pathname, "core", "include")

	def get_include_dirs(self):
		return self._include_dirs + [self.get_numpy_incpath()]

	def set_include_dirs(self, value):
		self._include_dirs = value

	def del_include_dirs(self):
		pass

	include_dirs = property(get_include_dirs, set_include_dirs, del_include_dirs)

# Being DRY adept and generating header file with C structures
generate_header = get_symbol(('dcmt', 'structures.py'), 'generate_header')
header = generate_header()
with open(os.path.join(setup_dir, 'src', 'wrapper', 'structures.h'), 'w') as f:
	f.write(header)

libdcmt = NumpyExtension('dcmt._libdcmt',
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

setup(
	name='dcmt',
	packages=['dcmt'],
	ext_modules=[libdcmt],
	install_requires=['numpy'],
	version=VERSION,
	author='Bogdan Opanchuk',
	author_email='mantihor@gmail.com',
	url='http://github.com/Manticore/python-dcmt',
	description='Dynamical creation of Mersenne twisters',
	long_description=DOCUMENTATION,
	classifiers=[
		'Development Status :: 4 - Beta',
		'Intended Audience :: Developers',
		'License :: OSI Approved :: MIT License',
		'Operating System :: OS Independent',
		'Programming Language :: Python :: 2',
		'Topic :: Scientific/Engineering :: Mathematics'
	]
)
