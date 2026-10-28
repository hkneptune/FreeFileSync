SHELL := /bin/bash
.SHELLFLAGS := -euo pipefail -c
#-e  exit script when last command fails
#-u  treat undefined variables as error
#-o pipefail  consider all individual calls when using pipes
#-c  execute the subsequent command string passed by Make
CXX ?= g++
exeName := RealTimeSync_$(shell arch)

CXXFLAGS += -std=c++26 -pipe -I../../.. -I../../../zenXml -include "zen/i18n.h" \
           -DWXINTL_NO_GETTEXT_MACRO -DwxNO_IMPLICIT_WXSTRING_ENCODING -DwxNO_IMPLICIT_WXSTRING_CONV_TO_PTR \
           -Wall -Wfatal-errors -Wshadow -Wmissing-include-dirs -Wswitch-enum -Wcast-align -Wnon-virtual-dtor -Wimplicit-fallthrough -Wundef -Wpedantic -Wredundant-decls \
           -Wno-unused-function -Wno-deprecated-enum-enum-conversion
           -O3 -DNDEBUG
CXXFLAGS += -Wno-maybe-uninitialized -Wno-free-nonheap-object -Wno-restrict -Wduplicated-cond -Wlogical-op

CXX += -pthread

CXXFLAGS += $(shell wx-config --cxxflags --debug=no)
LDFLAGS  += $(shell wx-config --libs std, aui, richtext --debug=no)

CXXFLAGS += $(shell pkg-config --cflags gtk+-3.0)
#treat as system headers so that warnings are hidden:
CXXFLAGS += -isystem/usr/include/gtk-3.0

cppFiles :=
cppFiles += application.cpp
cppFiles += config.cpp
cppFiles += gui_generated.cpp
cppFiles += main_dlg.cpp
cppFiles += tray_menu.cpp
cppFiles += monitor.cpp
cppFiles += folder_selector2.cpp
cppFiles += ../afs/abstract.cpp
cppFiles += ../base/icon_loader.cpp
cppFiles += ../ffs_paths.cpp
cppFiles += ../icon_buffer.cpp
cppFiles += ../localization.cpp
cppFiles += ../../../wx+/darkmode.cpp
cppFiles += ../../../wx+/file_drop.cpp
cppFiles += ../../../wx+/image_tools.cpp
cppFiles += ../../../wx+/image_resources.cpp
cppFiles += ../../../wx+/popup_dlg.cpp
cppFiles += ../../../wx+/popup_dlg_generated.cpp
cppFiles += ../../../wx+/taskbar.cpp
cppFiles += ../../../xBRZ/src/xbrz.cpp
cppFiles += ../../../zen/dir_watcher.cpp
cppFiles += ../../../zen/file_access.cpp
cppFiles += ../../../zen/file_io.cpp
cppFiles += ../../../zen/file_path.cpp
cppFiles += ../../../zen/file_traverser.cpp
cppFiles += ../../../zen/format_unit.cpp
cppFiles += ../../../zen/legacy_compiler.cpp
cppFiles += ../../../zen/resolve_path.cpp
cppFiles += ../../../zen/process_exec.cpp
cppFiles += ../../../zen/shutdown.cpp
cppFiles += ../../../zen/sys_error.cpp
cppFiles += ../../../zen/sys_info.cpp
cppFiles += ../../../zen/sys_version.cpp
cppFiles += ../../../zen/thread.cpp
cppFiles += ../../../zen/zstring.cpp

tmpPath := $(shell dirname "$(shell mktemp -u)")/$(exeName)_Make

objFiles := $(cppFiles:%=$(tmpPath)/ffs/src/rts/%.o)

all: ../../Build/Bin/$(exeName)

../../Build/Bin/$(exeName): $(objFiles)
	mkdir -p $(dir $@)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(tmpPath)/ffs/src/rts/%.o : %
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(tmpPath)
	rm -f ../../Build/Bin/$(exeName)
