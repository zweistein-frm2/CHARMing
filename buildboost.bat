cd boost
del /s /q out

b2 --clean
del bin.v2/config.log
del bin.v2/project-cache.jam


call bootstrap.bat --with-python=python3 --without-icu --prefix=%CD%\out
b2 link=static,shared cxxflags=-std:c++17 address-model=64 --without-wave --without-test --disable-icu boost.locale.icu=off boost.locale.iconv=on install --prefix=%CD%\out
cd ..