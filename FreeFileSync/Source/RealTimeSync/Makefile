APPNAME = RealTimeSync
prefix = /usr
BINDIR = $(DESTDIR)$(prefix)/bin

CXXFLAGS  = -std=c++14 -pipe -DWXINTL_NO_GETTEXT_MACRO -I../../.. -I../../../zenXml -include "zen/i18n.h" -Wall -O3 -DNDEBUG `wx-config --cxxflags --debug=no` -pthread

LINKFLAGS = -s `wx-config --libs std, aui --debug=no` -lboost_thread -lboost_chrono -lboost_system -lz -pthread

#Gtk - support "no button border"
CXXFLAGS  += `pkg-config --cflags gtk+-2.0`
LINKFLAGS += `pkg-config --libs   gtk+-2.0`

CPP_LIST=
CPP_LIST+=application.cpp
CPP_LIST+=gui_generated.cpp
CPP_LIST+=main_dlg.cpp
CPP_LIST+=tray_menu.cpp
CPP_LIST+=monitor.cpp
CPP_LIST+=xml_proc.cpp
CPP_LIST+=folder_selector2.cpp
CPP_LIST+=../structures.cpp
CPP_LIST+=../lib/localization.cpp
CPP_LIST+=../lib/process_xml.cpp
CPP_LIST+=../lib/resolve_path.cpp
CPP_LIST+=../lib/ffs_paths.cpp
CPP_LIST+=../lib/hard_filter.cpp
CPP_LIST+=../../../zen/xml_io.cpp
CPP_LIST+=../../../zen/dir_watcher.cpp
CPP_LIST+=../../../zen/file_access.cpp
CPP_LIST+=../../../zen/file_io.cpp
CPP_LIST+=../../../zen/file_traverser.cpp
CPP_LIST+=../../../zen/zstring.cpp
CPP_LIST+=../../../zen/format_unit.cpp
CPP_LIST+=../../../wx+/file_drop.cpp
CPP_LIST+=../../../wx+/image_tools.cpp
CPP_LIST+=../../../wx+/image_resources.cpp
CPP_LIST+=../../../wx+/popup_dlg.cpp
CPP_LIST+=../../../wx+/popup_dlg_generated.cpp

OBJECT_LIST=$(CPP_LIST:%.cpp=../../Obj/RTS_GCC_Make_Release/ffs/src/rts/%.o)

all: launchpad

launchpad: RealTimeSync

../../Obj/RTS_GCC_Make_Release/ffs/src/rts/%.o : %.cpp
	mkdir -p $(dir $@)
	g++ $(CXXFLAGS) -c $< -o $@

RealTimeSync: $(OBJECT_LIST)
	g++ -o ../../Build/$(APPNAME) $(OBJECT_LIST) $(LINKFLAGS)

clean:
	rm -rf ../../Obj/RTS_GCC_Make_Release
	rm -f ../../Build/$(APPNAME)
	rm -f ../../../wx+/pch.h.gch

install:
	mkdir -p $(BINDIR)
	cp ../../Build/$(APPNAME) $(BINDIR)
