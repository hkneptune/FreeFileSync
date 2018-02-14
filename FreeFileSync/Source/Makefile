APPNAME     = FreeFileSync
prefix      = /usr
BINDIR      = $(DESTDIR)$(prefix)/bin
SHAREDIR    = $(DESTDIR)$(prefix)/share
APPSHAREDIR = $(SHAREDIR)/$(APPNAME)
DOCSHAREDIR = $(SHAREDIR)/doc/$(APPNAME)

CXXFLAGS  = -std=c++14 -pipe -DWXINTL_NO_GETTEXT_MACRO -I../.. -I../../zenXml -include "zen/i18n.h" -Wall -O3 -DNDEBUG `wx-config --cxxflags --debug=no` -pthread

LINKFLAGS = -s `wx-config --libs std, aui --debug=no` -lboost_thread -lboost_chrono -lboost_system -lz -pthread

#Gtk - support recycler/icon loading/no button border/grid scrolling
CXXFLAGS  += `pkg-config --cflags gtk+-2.0`
LINKFLAGS += `pkg-config --libs   gtk+-2.0`

#support for SELinux (optional)
SELINUX_EXISTING=$(shell pkg-config --exists libselinux && echo YES)
ifeq ($(SELINUX_EXISTING),YES)
CXXFLAGS  += `pkg-config --cflags libselinux` -DHAVE_SELINUX
LINKFLAGS += `pkg-config --libs libselinux`
endif

#support for Ubuntu Unity (optional)
UNITY_EXISTING=$(shell pkg-config --exists unity && echo YES)
ifeq ($(UNITY_EXISTING),YES)
CXXFLAGS  += `pkg-config --cflags unity` -DHAVE_UBUNTU_UNITY
LINKFLAGS += `pkg-config --libs unity`
endif

CPP_LIST=
CPP_LIST+=algorithm.cpp
CPP_LIST+=application.cpp
CPP_LIST+=comparison.cpp
CPP_LIST+=structures.cpp
CPP_LIST+=synchronization.cpp
CPP_LIST+=fs/abstract.cpp
CPP_LIST+=fs/concrete.cpp
CPP_LIST+=fs/native.cpp
CPP_LIST+=file_hierarchy.cpp
CPP_LIST+=ui/cfg_grid.cpp
CPP_LIST+=ui/file_grid.cpp
CPP_LIST+=ui/folder_history_box.cpp
CPP_LIST+=ui/command_box.cpp
CPP_LIST+=ui/folder_selector.cpp
CPP_LIST+=ui/batch_config.cpp
CPP_LIST+=ui/batch_status_handler.cpp
CPP_LIST+=ui/version_check.cpp
CPP_LIST+=ui/file_view.cpp
CPP_LIST+=ui/tree_grid.cpp
CPP_LIST+=ui/gui_generated.cpp
CPP_LIST+=ui/gui_status_handler.cpp
CPP_LIST+=ui/main_dlg.cpp
CPP_LIST+=ui/progress_indicator.cpp
CPP_LIST+=ui/search.cpp
CPP_LIST+=ui/small_dlgs.cpp
CPP_LIST+=ui/sync_cfg.cpp
CPP_LIST+=ui/taskbar.cpp
CPP_LIST+=ui/triple_splitter.cpp
CPP_LIST+=ui/tray_icon.cpp
CPP_LIST+=lib/binary.cpp
CPP_LIST+=lib/db_file.cpp
CPP_LIST+=lib/dir_lock.cpp
CPP_LIST+=lib/hard_filter.cpp
CPP_LIST+=lib/icon_buffer.cpp
CPP_LIST+=lib/icon_loader.cpp
CPP_LIST+=lib/localization.cpp
CPP_LIST+=lib/parallel_scan.cpp
CPP_LIST+=lib/process_xml.cpp
CPP_LIST+=lib/resolve_path.cpp
CPP_LIST+=lib/perf_check.cpp
CPP_LIST+=lib/status_handler.cpp
CPP_LIST+=lib/versioning.cpp
CPP_LIST+=lib/ffs_paths.cpp
CPP_LIST+=../../zen/xml_io.cpp
CPP_LIST+=../../zen/recycler.cpp
CPP_LIST+=../../zen/file_access.cpp
CPP_LIST+=../../zen/file_io.cpp
CPP_LIST+=../../zen/file_traverser.cpp
CPP_LIST+=../../zen/zstring.cpp
CPP_LIST+=../../zen/format_unit.cpp
CPP_LIST+=../../zen/process_priority.cpp
CPP_LIST+=../../zen/shutdown.cpp
CPP_LIST+=../../wx+/file_drop.cpp
CPP_LIST+=../../wx+/grid.cpp
CPP_LIST+=../../wx+/image_tools.cpp
CPP_LIST+=../../wx+/graph.cpp
CPP_LIST+=../../wx+/tooltip.cpp
CPP_LIST+=../../wx+/http.cpp
CPP_LIST+=../../wx+/image_resources.cpp
CPP_LIST+=../../wx+/popup_dlg.cpp
CPP_LIST+=../../wx+/popup_dlg_generated.cpp
CPP_LIST+=../../wx+/zlib_wrap.cpp

OBJECT_LIST = $(CPP_LIST:%.cpp=../Obj/FFS_GCC_Make_Release/ffs/src/%.o)

all: launchpad

launchpad: FreeFileSync

../Obj/FFS_GCC_Make_Release/ffs/src/%.o : %.cpp
	mkdir -p $(dir $@)
	g++ $(CXXFLAGS) -c $< -o $@

FreeFileSync: $(OBJECT_LIST)
	g++ -o ../Build/$(APPNAME) $(OBJECT_LIST) $(LINKFLAGS)

clean:
	rm -rf ../Obj/FFS_GCC_Make_Release
	rm -f ../Build/$(APPNAME)
	rm -f ../../wx+/pch.h.gch

install:
	mkdir -p $(BINDIR)
	cp ../Build/$(APPNAME) $(BINDIR)

	mkdir -p $(APPSHAREDIR)
	cp -R ../Build/Languages/ \
	../Build/Help/ \
	../Build/ding.wav \
	../Build/gong.wav \
	../Build/harp.wav \
	../Build/Resources.zip \
	$(APPSHAREDIR)

	mkdir -p $(DOCSHAREDIR)
	cp ../Build/Changelog.txt $(DOCSHAREDIR)/changelog
	gzip $(DOCSHAREDIR)/changelog
