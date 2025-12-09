from numpy import get_include as np_include
from setuptools import Extension, setup

ext_modules = [
    Extension(
        "stsci.image._combine",
        ["src/_combinemodule.c"],
        include_dirs=[np_include()],
    ),
]

setup(
    ext_modules=ext_modules,
)
