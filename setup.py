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

setup_dir = os.path.split(os.path.abspath(__file__))[0]
DOCUMENTATION = open(os.path.join(setup_dir, 'README.rst')).read()

dcmt_path = os.path.join(setup_dir, 'dcmt', 'version.py')
globals_dict = {}
execfile(dcmt_path, globals_dict)
VERSION = globals_dict['VERSION_STRING']

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

setup(
	name='dcmt',
	packages=['dcmt'],
	ext_modules = [libdcmt],
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
