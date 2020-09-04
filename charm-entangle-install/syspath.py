import sys
from distutils.sysconfig import get_python_lib
print(get_python_lib())
print(get_python_lib(True,True))
print(get_python_lib(True,False))
print(get_python_lib(False,True))
print(get_python_lib(False,False))
print('\n'.join(sys.path))
