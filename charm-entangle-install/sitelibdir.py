import distutils.sysconfig
print(distutils.sysconfig.get_python_lib(plat_specific = False, standard_lib = False))
