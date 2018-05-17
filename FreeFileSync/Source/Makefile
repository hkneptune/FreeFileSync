APPNAME     = FreeFileSync
prefix      = /usr
BINDIR      = $(DESTDIR)$(prefix)/bin
SHAREDIR    = $(DESTDIR)$(prefix)/share
APPSHAREDIR = $(SHAREDIR)/$(APPNAME)
DOCSHAREDIR = $(SHAREDIR)/doc/$(APPNAME)

CXXFLAGS  = -std=c++17 -pipe -DWXINTL_NO_GETTEXT_MACRO -I../.. -I../../zenXml -I../../boost -include "zen/i18n.h" -include "zen/warn_static.h" \
-Wall -Wfatal-errors  -Winit-self -Wmissing-include-dirs -Wswitch-enum -Wmain -Wnon-virtual-dtor -Wcast-align -Wshadow -Wno-deprecated-declarations \
-O3 -DNDEBUG `wx-config --cxxflags --debug=no` -pthread

LINKFLAGS = -s -no-pie `wx-config --libs std, aui --debug=no` -pthread

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

CPP_FILES=
CPP_FILES+=algorithm.cpp
CPP_FILES+=application.cpp
CPP_FILES+=comparison.cpp
CPP_FILES+=structures.cpp
CPP_FILES+=synchronization.cpp
CPP_FILES+=fs/abstract.cpp
CPP_FILES+=fs/concrete.cpp
CPP_FILES+=fs/native.cpp
CPP_FILES+=file_hierarchy.cpp
CPP_FILES+=ui/batch_config.cpp
CPP_FILES+=ui/batch_status_handler.cpp
CPP_FILES+=ui/cfg_grid.cpp
CPP_FILES+=ui/command_box.cpp
CPP_FILES+=ui/folder_history_box.cpp
CPP_FILES+=ui/folder_selector.cpp
CPP_FILES+=ui/file_grid.cpp
CPP_FILES+=ui/file_view.cpp
CPP_FILES+=ui/tree_grid.cpp
CPP_FILES+=ui/gui_generated.cpp
CPP_FILES+=ui/gui_status_handler.cpp
CPP_FILES+=ui/main_dlg.cpp
CPP_FILES+=ui/progress_indicator.cpp
CPP_FILES+=ui/search.cpp
CPP_FILES+=ui/small_dlgs.cpp
CPP_FILES+=ui/sync_cfg.cpp
CPP_FILES+=ui/taskbar.cpp
CPP_FILES+=ui/tray_icon.cpp
CPP_FILES+=ui/triple_splitter.cpp
CPP_FILES+=ui/version_check.cpp
CPP_FILES+=lib/binary.cpp
CPP_FILES+=lib/db_file.cpp
CPP_FILES+=lib/dir_lock.cpp
CPP_FILES+=lib/ffs_paths.cpp
CPP_FILES+=lib/generate_logfile.cpp
CPP_FILES+=lib/hard_filter.cpp
CPP_FILES+=lib/icon_buffer.cpp
CPP_FILES+=lib/icon_loader.cpp
CPP_FILES+=lib/localization.cpp
CPP_FILES+=lib/parallel_scan.cpp
CPP_FILES+=lib/process_xml.cpp
CPP_FILES+=lib/resolve_path.cpp
CPP_FILES+=lib/perf_check.cpp
CPP_FILES+=lib/status_handler.cpp
CPP_FILES+=lib/versioning.cpp
CPP_FILES+=../../zen/xml_io.cpp
CPP_FILES+=../../zen/recycler.cpp
CPP_FILES+=../../zen/file_access.cpp
CPP_FILES+=../../zen/file_io.cpp
CPP_FILES+=../../zen/file_traverser.cpp
CPP_FILES+=../../zen/zstring.cpp
CPP_FILES+=../../zen/format_unit.cpp
CPP_FILES+=../../zen/process_priority.cpp
CPP_FILES+=../../zen/shutdown.cpp
CPP_FILES+=../../wx+/file_drop.cpp
CPP_FILES+=../../wx+/grid.cpp
CPP_FILES+=../../wx+/image_tools.cpp
CPP_FILES+=../../wx+/graph.cpp
CPP_FILES+=../../wx+/http.cpp
CPP_FILES+=../../wx+/tooltip.cpp
CPP_FILES+=../../wx+/image_resources.cpp
CPP_FILES+=../../wx+/popup_dlg.cpp
CPP_FILES+=../../wx+/popup_dlg_generated.cpp
CPP_FILES+=../../wx+/zlib_wrap.cpp
CPP_FILES+=../../xBRZ/src/xbrz.cpp

OBJECT_LIST = $(CPP_FILES:%.cpp=../Obj/FFS_GCC_Make_Release/ffs/src/%.o)

all: ../Build/$(APPNAME)

../Build/$(APPNAME): $(OBJECT_LIST)
	g++ -o $@ $^ $(LINKFLAGS)

../Obj/FFS_GCC_Make_Release/ffs/src/%.o : %.cpp
	mkdir -p $(dir $@)
	g++ $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf ../Obj/FFS_GCC_Make_Release
	rm -f ../Build/$(APPNAME)

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
