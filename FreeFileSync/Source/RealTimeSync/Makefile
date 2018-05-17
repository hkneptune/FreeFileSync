APPNAME = RealTimeSync
prefix = /usr
BINDIR = $(DESTDIR)$(prefix)/bin

CXXFLAGS  = -std=c++17 -pipe -DWXINTL_NO_GETTEXT_MACRO -I../../.. -I../../../zenXml -I../../../boost -include "zen/i18n.h" -include "zen/warn_static.h" \
-Wall -Wfatal-errors  -Winit-self -Wmissing-include-dirs -Wswitch-enum -Wmain -Wnon-virtual-dtor -Wcast-align -Wshadow -Wno-deprecated-declarations \
-O3 -DNDEBUG `wx-config --cxxflags --debug=no` -pthread

LINKFLAGS = -s -no-pie `wx-config --libs std, aui --debug=no` -pthread

#Gtk - support "no button border"
CXXFLAGS  += `pkg-config --cflags gtk+-2.0`
LINKFLAGS += `pkg-config --libs   gtk+-2.0`

CPP_FILES=
CPP_FILES+=application.cpp
CPP_FILES+=gui_generated.cpp
CPP_FILES+=main_dlg.cpp
CPP_FILES+=tray_menu.cpp
CPP_FILES+=monitor.cpp
CPP_FILES+=xml_proc.cpp
CPP_FILES+=folder_selector2.cpp
CPP_FILES+=../lib/localization.cpp
CPP_FILES+=../lib/resolve_path.cpp
CPP_FILES+=../lib/ffs_paths.cpp
CPP_FILES+=../../../zen/xml_io.cpp
CPP_FILES+=../../../zen/dir_watcher.cpp
CPP_FILES+=../../../zen/file_access.cpp
CPP_FILES+=../../../zen/file_io.cpp
CPP_FILES+=../../../zen/file_traverser.cpp
CPP_FILES+=../../../zen/zstring.cpp
CPP_FILES+=../../../zen/format_unit.cpp
CPP_FILES+=../../../wx+/file_drop.cpp
CPP_FILES+=../../../wx+/image_tools.cpp
CPP_FILES+=../../../wx+/image_resources.cpp
CPP_FILES+=../../../wx+/popup_dlg.cpp
CPP_FILES+=../../../wx+/popup_dlg_generated.cpp
CPP_FILES+=../../../xBRZ/src/xbrz.cpp

OBJECT_LIST=$(CPP_FILES:%.cpp=../../Obj/RTS_GCC_Make_Release/ffs/src/rts/%.o)

all: ../../Build/$(APPNAME)

../../Build/$(APPNAME): $(OBJECT_LIST)
	g++ -o $@ $^ $(LINKFLAGS)

../../Obj/RTS_GCC_Make_Release/ffs/src/rts/%.o : %.cpp
	mkdir -p $(dir $@)
	g++ $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf ../../Obj/RTS_GCC_Make_Release
	rm -f ../../Build/$(APPNAME)

install:
	mkdir -p $(BINDIR)
	cp ../../Build/$(APPNAME) $(BINDIR)
