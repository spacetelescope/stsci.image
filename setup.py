#!/usr/bin/env python
import relic.release
from glob import glob
from numpy import get_include as np_include
from setuptools import setup, find_packages, Extension


version = relic.release.get_info()
relic.release.write_template(version, 'stsci/image')

setup(
    name = 'stsci.image',
    version = version.pep386,
    author = 'Todd Miller',
    author_email = 'help@stsci.edu',
    description = 'Image array manipulation functions',
    url = 'https://github.com/spacetelescope/stsci.image',
    classifiers = [
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: BSD License',
        'Operating System :: OS Independent',
        'Programming Language :: Python',
        'Topic :: Scientific/Engineering :: Astronomy',
        'Topic :: Software Development :: Libraries :: Python Modules',
    ],
    install_requires = [
        'astropy',
        'nose',
        'numpy',
        'scipy',
        'stsci.tools'
    ],
    packages = find_packages(),
    package_data = {
        '': ['LICENSE.txt'],
        'stsci/image/test': ['*.fits']
    },
    ext_modules=[
        Extension('stsci.image._combine',
            ['src/_combinemodule.c'],
            include_dirs=[np_include()],
            define_macros=[('NUMPY','1')]),
    ],
)
