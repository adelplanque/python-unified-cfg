# -*- coding: utf-8 -*-
from setuptools import setup, Extension
import subprocess
import sys


def pkgconfig_configure(ext, packages):
    cflags = subprocess.check_output(["pkg-config", "--cflags"] + packages)
    for flag in cflags.decode("utf-8").split():
        if flag.startswith("-I"):
            path = flag[2:]
            if path not in ext.include_dirs:
                ext.include_dirs.append(path)
        else:
            ext.extra_compile_args.append(flag)

    libs = subprocess.check_output(["pkg-config", "--libs"] + packages)
    for flag in libs.decode("utf-8").split():
        if flag.startswith("-l"):
            ext.libraries.append(flag[2:])
        elif flag.startswith("-L"):
            ext.library_dirs.append(flag[2:])
        else:
            ext.extra_link_args.append(flag)


if sys.version_info[0] == 2:
    pybind11_include = "ext/pybind11-2.9.2/include"
else:
    pybind11_include = "ext/pybind11-2.10.4/include"


_config_manager = Extension(
    "cfg._cfg",
    sources=["src/cfg.cpp"],
    extra_compile_args=[
        "-std=c++17",
        "-ffunction-sections",
        "-fdata-sections",
        "-flto=auto",
    ],
    include_dirs=[pybind11_include],
    libraries=["stdc++fs"],
    extra_link_args=[
        "-flto=auto",
        "-Wl,--gc-sections",
        "-Wl,--version-script=version-script-py%i" % sys.version_info[0],
    ],
)


if "sdist" not in sys.argv:
    pkgconfig_configure(_config_manager, ["cfg",])


setup(
    name="python-unified-cfg",
    version="0.1.0",
    packages=["cfg"],
    ext_modules=[_config_manager],
    install_requires=(
        ["pathlib2", "importlib_resources"] if sys.version_info[0] == 2 else []
    ),
)
