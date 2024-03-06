#!/bin/sh
#
# Modified from https://github.com/kupiqu/SierraBreezeEnhanced
# - Uninstall previous version if exists
# - Recreate build
# - Build
# - Install

uninstall() {
    sudo make uninstall && echo "Uninstalled successfully!"
}

ORIGINAL_DIR=$(pwd)

if [ -d "build" ]; then #uninstall any previous version first
    if test -f "$ORIGINAL_DIR/build/install_manifest.txt"; then
        echo Found $ORIGINAL_DIR/build/install_manifest.txt from previous installation, uninstalling first
        cd build &&
        uninstall
    fi
    cd $ORIGINAL_DIR
    sudo rm -rf build
fi

cd $ORIGINAL_DIR
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DKDE_INSTALL_USE_QT_SYS_PATHS=ON
make -j$(nproc)
sudo make install && echo "Installed successfully!"

cd $ORIGINAL_DIR
