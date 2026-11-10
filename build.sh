#!/bin/sh
# Cross-build librmpp-bt.so and rmpp-bt.rcc. SDK defaults to ../sdk (Arm GNU aarch64 + Qt 6.10 headers + device libs).
set -e
cd "$(dirname "$0")"
SDK=${SDK:-../sdk}
CXX=${CXX:-$SDK/bin/aarch64-none-linux-gnu-g++}
Q=$SDK/qt/usr/include/qt6
mkdir -p build
"$CXX" -std=c++17 -O2 -fPIC -shared -DQT_NO_VERSION_TAGGING -DQT_NO_DEBUG \
  -I"$Q" -I"$Q/QtCore" -I"$Q/QtGui" -I"$Q/QtQml" -I"$Q/QtQuick" -I"$Q/QtDBus" \
  src/rmppbt.cpp -o build/librmpp-bt.so -L"$SDK/devlibs" \
  -l:libQt6Core.so.6 -l:libQt6Gui.so.6 -l:libQt6Qml.so.6 -l:libQt6Quick.so.6 -l:libQt6DBus.so.6 \
  -Wl,--allow-shlib-undefined -Wl,-soname,librmpp-bt.so
${RCC:-pyside6-rcc} --binary --no-compress rmpp-bt.qrc -o build/rmpp-bt.rcc
ls -la build
