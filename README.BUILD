Sequence to build and package everything including the installer:


1. Build entangle-charming
   cd CHARMing/entangle-charming
   mkdir out
   cd out
   cmake ..  (Windows: cmake -DCMAKE_BUILD_TYPE=Release ..) (first cmake call takes a long time as static libraries are build with correct parameters )
   make      (Windows: cmake --build . --config Release)


2. Build CHARMing (includes charm and entangle-install-charming)
   export INSTALL_DEPS=libgtk-3-0  (here for ubuntu18, see ci/Dockerfile.* for other, not on Windows )
   export LINUX_FLAVOUR=ubuntu18   (not on Windows)
   cd CHARMing
   mkdir out
   cd out
   cmake .. (Windows: cmake -DCMAKE_BUILD_TYPE=Release ..)
   make package (this builds the installer package .deb) (Windows: cmake --build . --config Release)
