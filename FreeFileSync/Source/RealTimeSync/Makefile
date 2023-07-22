CXX ?= g++
exeName = RealTimeSync_$(shell arch)

CXXFLAGS += -std=c++23 -pipe -DWXINTL_NO_GETTEXT_MACRO -I../../.. -I../../../zenXml -include "zen/i18n.h" -include "zen/warn_static.h" \
           -Wall -Wfatal-errors -Wmissing-include-dirs -Wswitch-enum -Wcast-align -Wnon-virtual-dtor -Wno-unused-function -Wshadow -Wno-maybe-uninitialized \
           -O3 -DNDEBUG `wx-config --cxxflags --debug=no` -pthread

LDFLAGS += -s -no-pie `wx-config --libs std, aui, richtext --debug=no` -pthread

#Gtk - support "no button border"
CXXFLAGS  += `pkg-config --cflags gtk+-2.0`
#treat as system headers so that warnings are hidden:
CXXFLAGS  += -isystem/usr/include/gtk-2.0

cppFiles=
cppFiles+=application.cpp
cppFiles+=config.cpp
cppFiles+=gui_generated.cpp
cppFiles+=main_dlg.cpp
cppFiles+=tray_menu.cpp
cppFiles+=monitor.cpp
cppFiles+=folder_selector2.cpp
cppFiles+=../afs/abstract.cpp
cppFiles+=../base/icon_loader.cpp
cppFiles+=../ffs_paths.cpp
cppFiles+=../icon_buffer.cpp
cppFiles+=../localization.cpp
cppFiles+=../../../wx+/file_drop.cpp
cppFiles+=../../../wx+/image_tools.cpp
cppFiles+=../../../wx+/image_resources.cpp
cppFiles+=../../../wx+/popup_dlg.cpp
cppFiles+=../../../wx+/popup_dlg_generated.cpp
cppFiles+=../../../wx+/taskbar.cpp
cppFiles+=../../../xBRZ/src/xbrz.cpp
cppFiles+=../../../zen/dir_watcher.cpp
cppFiles+=../../../zen/file_access.cpp
cppFiles+=../../../zen/file_io.cpp
cppFiles+=../../../zen/file_path.cpp
cppFiles+=../../../zen/file_traverser.cpp
cppFiles+=../../../zen/format_unit.cpp
cppFiles+=../../../zen/legacy_compiler.cpp
cppFiles+=../../../zen/resolve_path.cpp
cppFiles+=../../../zen/process_exec.cpp
cppFiles+=../../../zen/shutdown.cpp
cppFiles+=../../../zen/sys_error.cpp
cppFiles+=../../../zen/sys_info.cpp
cppFiles+=../../../zen/sys_version.cpp
cppFiles+=../../../zen/thread.cpp
cppFiles+=../../../zen/zstring.cpp

tmpPath = $(shell dirname "$(shell mktemp -u)")/$(exeName)_Make

objFiles = $(cppFiles:%=$(tmpPath)/ffs/src/rts/%.o)

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
