import os
import platform
import ctypes

def load_libpaf():
    base_dir = os.path.dirname(__file__)
    system = platform.system()

    if system == 'Windows':
        libname = 'libpaf.dll'
    elif system == 'Darwin':
        libname = 'libpaf.dylib'
    else:
        libname = 'libpaf.so'

    libpath = os.path.join(base_dir, libname)
    return ctypes.CDLL(libpath)

libpaf = load_libpaf()
