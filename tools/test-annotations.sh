#!/bin/bash
set -euo pipefail
ulimit -c 0
task_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$task_root"
backend=${DTP_TEST_SUDACHI:-0}
toolchain=/opt/kobo-double-tap/nickeltc
sysroot="$toolchain/arm-nickel-linux-gnueabihf/sysroot"
export PATH="$toolchain/bin:$PATH"
mkdir -p build/native-test
python3 tools/embed-japanese.py
moc tests/annotation_qt_test.h -o build/native-test/annotation-qt-test.moc
arm-nickel-linux-gnueabihf-g++ -std=gnu++11 -O2 -fPIC -march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=hard -mthumb -Wall -Wextra -Werror \
    -Ivendor/NickelHook -Ibuild/native-test \
    '-DDTP_ANNOTATION_REPORT="build/native-test/report"' \
    '-DDTP_SETTINGS_FILE="build/native-test/settings"' \
    -DDTP_TEST_MANUAL_MENU -DDTP_USE_SUDACHI=$backend \
    tests/annotation_qt_test.cc tests/sudachi_stub.cc src/annotations.cc src/toggle.cc src/japanese.cc src/rendering.cc src/settings.cc -o build/native-test/annotation-qt-test \
    $(arm-nickel-linux-gnueabihf-pkg-config --cflags --libs Qt5Widgets)
qemu-arm -L /usr/arm-linux-gnueabihf \
    -E "LD_LIBRARY_PATH=/usr/arm-linux-gnueabihf/lib:$sysroot/usr/lib" \
    -E QT_QPA_PLATFORM=minimal \
    -E "QT_QPA_PLATFORM_PLUGIN_PATH=$sysroot/usr/plugins/platforms" \
    build/native-test/annotation-qt-test
