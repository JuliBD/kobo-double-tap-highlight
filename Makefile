include vendor/NickelHook/NickelHook.mk

override LIBRARY := src/libkobodoubletapprobe.so
override SOURCES += src/probe.c src/report.c src/probe_clock.c src/double_tap.c src/uninstall.c
override SOURCES += src/annotations.cc src/toggle.cc src/japanese.cc src/rendering.cc src/settings.cc
override PKGCONF += Qt5Widgets
override VERSION := prototype-0.8.0-beta.3
override LDFLAGS += -lrt
SUDACHI ?= 0
override CPPFLAGS += -DDTP_USE_SUDACHI=$(SUDACHI)
ifeq ($(SUDACHI),1)
override SOURCES += src/sudachi.cc
override LDFLAGS += build/sudachi-native/armv7-unknown-linux-gnueabihf/release/libdtp_sudachi.a -lm -Wl,--exclude-libs,ALL
endif
override CFLAGS += -Wall -Wextra -Werror -fvisibility=hidden
override CXXFLAGS += -Wall -Wextra -Werror -Wno-missing-field-initializers -fvisibility=hidden

include vendor/NickelHook/NickelHook.mk

# NickelHook passes all prerequisites to its compiler helper. Keep header
# dependencies for rebuilding, but compile only the C translation unit.
override nh_cmd_c = $(CC) $(CPPFLAGS) $(CFLAGS) -c $(filter %.c,$(2)) -o $(1)
override nh_cmd_cc = $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(filter %.cc %.cpp,$(2)) -o $(1)

src/probe.o src/report.o: src/report.h
src/probe.o src/probe_clock.o: src/probe_clock.h
src/probe.o src/double_tap.o: src/double_tap.h
src/probe.o src/annotations.o: src/annotations.h
src/annotations.o: src/report.h
src/probe.o src/toggle.o: src/toggle.h
src/toggle.o: src/annotations.h src/report.h src/double_tap.h
src/probe.o src/toggle.o src/japanese.o src/annotations.o: src/japanese.h
src/japanese.o: build/japanese_bundle.h src/report.h
src/japanese.o src/sudachi.o: src/sudachi.h src/japanese_backend.h
src/probe.o: src/japanese_backend.h src/sudachi.h
ifeq ($(SUDACHI),1)
$(LIBRARY): build/sudachi-native/armv7-unknown-linux-gnueabihf/release/libdtp_sudachi.a
build/sudachi-native/armv7-unknown-linux-gnueabihf/release/libdtp_sudachi.a: native/sudachi-bridge/src/lib.rs native/sudachi-bridge/Cargo.toml native/sudachi-bridge/Cargo.lock
	bash tools/build-sudachi.sh --locked
endif
src/probe.o src/toggle.o src/rendering.o: src/rendering.h
src/rendering.o: src/annotations.h src/toggle.h src/report.h
build/japanese_bundle.h: tools/embed-japanese.py src/japanese-selection.js vendor/TinySegmenter/tinysegmenter.js vendor/TinySegmenter/LICENSE-BSD vendor/TinySegmenter/LICENSE-MIT
	python3 tools/embed-japanese.py
# Compiler definitions (including NH_VERSION) change when this file changes.
$(OBJECTS_C) $(OBJECTS_CXX) $(OBJECTS_CXX1) $(OBJECTS_MOC): Makefile

src/settings.o src/probe.o src/annotations.o: src/settings.h
src/settings.o: src/toggle.h src/japanese.h src/annotations.h

src/probe.o src/uninstall.o: src/uninstall.h
