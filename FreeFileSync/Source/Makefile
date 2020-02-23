EXENAME = FreeFileSync_$(shell arch)

CXXFLAGS = -std=c++2a -pipe -DWXINTL_NO_GETTEXT_MACRO -I../.. -I../../zenXml -include "zen/i18n.h" -include "zen/warn_static.h" \
           -Wall -Wfatal-errors -Wmissing-include-dirs -Wswitch-enum -Wcast-align -Wshadow -Wnon-virtual-dtor \
           -O3 -DNDEBUG `wx-config --cxxflags --debug=no` -pthread

LINKFLAGS = -s -no-pie `wx-config --libs std, aui --debug=no` -pthread


CXXFLAGS  += `pkg-config --cflags openssl`
LINKFLAGS += `pkg-config --libs   openssl`

CXXFLAGS  += `pkg-config --cflags libcurl`
LINKFLAGS += `pkg-config --libs   libcurl`

CXXFLAGS  += `pkg-config --cflags libssh2`
LINKFLAGS += `pkg-config --libs   libssh2`

CXXFLAGS  += `pkg-config --cflags gtk+-2.0`
#treat as system headers so that warnings are hidden:
CXXFLAGS  += -isystem/usr/include/gtk-2.0

#support for SELinux (optional)
SELINUX_EXISTING=$(shell pkg-config --exists libselinux && echo YES)
ifeq ($(SELINUX_EXISTING),YES)
CXXFLAGS  += `pkg-config --cflags libselinux` -DHAVE_SELINUX
LINKFLAGS += `pkg-config --libs libselinux`
endif

CPP_FILES=
CPP_FILES+=base/algorithm.cpp
CPP_FILES+=base/application.cpp
CPP_FILES+=base/binary.cpp
CPP_FILES+=base/comparison.cpp
CPP_FILES+=base/config.cpp
CPP_FILES+=base/db_file.cpp
CPP_FILES+=base/dir_lock.cpp
CPP_FILES+=base/ffs_paths.cpp
CPP_FILES+=base/file_hierarchy.cpp
CPP_FILES+=base/icon_buffer.cpp
CPP_FILES+=base/icon_loader.cpp
CPP_FILES+=base/localization.cpp
CPP_FILES+=base/log_file.cpp
CPP_FILES+=base/parallel_scan.cpp
CPP_FILES+=base/path_filter.cpp
CPP_FILES+=base/perf_check.cpp
CPP_FILES+=base/resolve_path.cpp
CPP_FILES+=base/status_handler.cpp
CPP_FILES+=base/structures.cpp
CPP_FILES+=base/synchronization.cpp
CPP_FILES+=base/versioning.cpp
CPP_FILES+=afs/abstract.cpp
CPP_FILES+=afs/concrete.cpp
CPP_FILES+=afs/ftp.cpp
CPP_FILES+=afs/gdrive.cpp
CPP_FILES+=afs/init_curl_libssh2.cpp
CPP_FILES+=afs/native.cpp
CPP_FILES+=afs/sftp.cpp
CPP_FILES+=ui/batch_config.cpp
CPP_FILES+=ui/abstract_folder_picker.cpp
CPP_FILES+=ui/batch_status_handler.cpp
CPP_FILES+=ui/cfg_grid.cpp
CPP_FILES+=ui/command_box.cpp
CPP_FILES+=ui/folder_history_box.cpp
CPP_FILES+=ui/folder_selector.cpp
CPP_FILES+=ui/file_grid.cpp
CPP_FILES+=ui/file_view.cpp
CPP_FILES+=ui/log_panel.cpp
CPP_FILES+=ui/tree_grid.cpp
CPP_FILES+=ui/gui_generated.cpp
CPP_FILES+=ui/gui_status_handler.cpp
CPP_FILES+=ui/main_dlg.cpp
CPP_FILES+=ui/progress_indicator.cpp
CPP_FILES+=ui/search_grid.cpp
CPP_FILES+=ui/small_dlgs.cpp
CPP_FILES+=ui/sync_cfg.cpp
CPP_FILES+=ui/taskbar.cpp
CPP_FILES+=ui/tray_icon.cpp
CPP_FILES+=ui/triple_splitter.cpp
CPP_FILES+=ui/version_check.cpp
CPP_FILES+=../../libcurl/rest.cpp
CPP_FILES+=../../zen/recycler.cpp
CPP_FILES+=../../zen/file_access.cpp
CPP_FILES+=../../zen/file_io.cpp
CPP_FILES+=../../zen/file_traverser.cpp
CPP_FILES+=../../zen/http.cpp
CPP_FILES+=../../zen/zstring.cpp
CPP_FILES+=../../zen/format_unit.cpp
CPP_FILES+=../../zen/legacy_compiler.cpp
CPP_FILES+=../../zen/open_ssl.cpp
CPP_FILES+=../../zen/process_priority.cpp
CPP_FILES+=../../zen/shutdown.cpp
CPP_FILES+=../../zen/system.cpp
CPP_FILES+=../../zen/thread.cpp
CPP_FILES+=../../zen/zlib_wrap.cpp
CPP_FILES+=../../wx+/file_drop.cpp
CPP_FILES+=../../wx+/grid.cpp
CPP_FILES+=../../wx+/image_tools.cpp
CPP_FILES+=../../wx+/graph.cpp
CPP_FILES+=../../wx+/tooltip.cpp
CPP_FILES+=../../wx+/image_resources.cpp
CPP_FILES+=../../wx+/popup_dlg.cpp
CPP_FILES+=../../wx+/popup_dlg_generated.cpp
CPP_FILES+=../../xBRZ/src/xbrz.cpp

TMP_PATH = /tmp/$(EXENAME)_Make

OBJ_FILES = $(CPP_FILES:%=$(TMP_PATH)/ffs/src/%.o)

all: ../Build/Bin/$(EXENAME)

../Build/Bin/$(EXENAME): $(OBJ_FILES)
	mkdir -p $(dir $@)
	g++ -o $@ $^ $(LINKFLAGS)

$(TMP_PATH)/ffs/src/%.o : %
	mkdir -p $(dir $@)
	g++ $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(TMP_PATH)
	rm -f ../Build/Bin/$(EXENAME)
