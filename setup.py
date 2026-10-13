from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension, build_ext

HEADERS = [
    "include/kerneless/utcet.h",
    "include/kerneless/dsk_reader.h",
]

ext_modules = [
    Pybind11Extension(
        "kerneless",
        ["python_bindings.cpp"],
        include_dirs=["include"],
        depends=HEADERS,
        cxx_std=11,
    ),
]

setup(
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    # Install the headers next to the extension module so C/C++ consumers can
    # `-I <prefix>/include` and `#include <kerneless/utcet.h>`. data_files are
    # laid down relative to the install prefix, which keeps the installed layout
    # identical to the repository's include/ tree.
    data_files=[("include/kerneless", HEADERS)],
    zip_safe=False,
)
