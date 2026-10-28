SHELL := /bin/bash
.SHELLFLAGS := -euo pipefail -c
#-e  exit script when last command fails
#-u  treat undefined variables as error
#-o pipefail  consider all individual calls when using pipes
#-c  execute the subsequent command string passed by Make
CXX ?= g++
exeName := FreeFileSync_$(shell arch)

CXXFLAGS += -std=c++26 -pipe -I../.. -I../../zenXml -include "zen/i18n.h" \
           -DWXINTL_NO_GETTEXT_MACRO -DwxNO_IMPLICIT_WXSTRING_ENCODING -DwxNO_IMPLICIT_WXSTRING_CONV_TO_PTR \
           -Wall -Wfatal-errors -Wshadow -Wmissing-include-dirs -Wswitch-enum -Wcast-align -Wnon-virtual-dtor -Wimplicit-fallthrough -Wundef -Wpedantic -Wredundant-decls \
           -Wno-unused-function -Wno-deprecated-enum-enum-conversion
           -O3 -DNDEBUG
CXXFLAGS += -Wno-maybe-uninitialized -Wno-free-nonheap-object -Wno-restrict -Wduplicated-cond -Wlogical-op

CXX += -pthread

CXXFLAGS += $(shell wx-config --cxxflags --debug=no)
LDFLAGS  += $(shell wx-config --libs std, aui, richtext --debug=no)

CXXFLAGS += $(shell pkg-config --cflags openssl)
LDFLAGS  += $(shell pkg-config --libs openssl)

CXXFLAGS += $(shell pkg-config --cflags libcurl)
LDFLAGS  += $(shell pkg-config --libs libcurl)

CXXFLAGS += $(shell pkg-config --cflags libidn2)
LDFLAGS  += $(shell pkg-config --libs libidn2)

CXXFLAGS += $(shell pkg-config --cflags libssh2)
LDFLAGS  += $(shell pkg-config --libs libssh2)

CXXFLAGS += $(shell pkg-config --cflags gtk+-3.0)
#treat as system headers so that warnings are hidden:
CXXFLAGS += -isystem/usr/include/gtk-3.0

#support for SELinux (optional)
haveSelinux := $(shell pkg-config --exists libselinux && echo YES)
ifeq ($(haveSelinux),YES)
    CXXFLAGS += $(shell pkg-config --cflags libselinux) -DHAVE_SELINUX
    LDFLAGS  += $(shell pkg-config --libs libselinux)
endif

cppFiles :=
cppFiles += application.cpp
cppFiles += base_tools.cpp
cppFiles += config.cpp
cppFiles += ffs_paths.cpp
cppFiles += icon_buffer.cpp
cppFiles += localization.cpp
cppFiles += log_file.cpp
cppFiles += status_handler.cpp
cppFiles += base/algorithm.cpp
cppFiles += base/binary.cpp
cppFiles += base/comparison.cpp
cppFiles += base/db_file.cpp
cppFiles += base/dir_lock.cpp
cppFiles += base/file_hierarchy.cpp
cppFiles += base/icon_loader.cpp
cppFiles += base/multi_rename.cpp
cppFiles += base/parallel_scan.cpp
cppFiles += base/path_filter.cpp
cppFiles += base/speed_test.cpp
cppFiles += base/structures.cpp
cppFiles += base/synchronization.cpp
cppFiles += base/versioning.cpp
cppFiles += afs/abstract.cpp
cppFiles += afs/concrete.cpp
cppFiles += afs/ftp.cpp
cppFiles += afs/gdrive.cpp
cppFiles += afs/init_curl_libssh2.cpp
cppFiles += afs/native.cpp
cppFiles += afs/sftp.cpp
cppFiles += ui/batch_config.cpp
cppFiles += ui/abstract_folder_picker.cpp
cppFiles += ui/batch_status_handler.cpp
cppFiles += ui/cfg_grid.cpp
cppFiles += ui/command_box.cpp
cppFiles += ui/folder_history_box.cpp
cppFiles += ui/folder_selector.cpp
cppFiles += ui/file_grid.cpp
cppFiles += ui/file_view.cpp
cppFiles += ui/log_panel.cpp
cppFiles += ui/tree_grid.cpp
cppFiles += ui/gui_generated.cpp
cppFiles += ui/gui_status_handler.cpp
cppFiles += ui/main_dlg.cpp
cppFiles += ui/progress_indicator.cpp
cppFiles += ui/rename_dlg.cpp
cppFiles += ui/search_grid.cpp
cppFiles += ui/small_dlgs.cpp
cppFiles += ui/sync_cfg.cpp
cppFiles += ui/tray_icon.cpp
cppFiles += ui/triple_splitter.cpp
cppFiles += ui/version_check.cpp
cppFiles += ../../libcurl/curl_wrap.cpp
cppFiles += ../../zen/argon2.cpp
cppFiles += ../../zen/file_access.cpp
cppFiles += ../../zen/file_io.cpp
cppFiles += ../../zen/file_path.cpp
cppFiles += ../../zen/file_traverser.cpp
cppFiles += ../../zen/http.cpp
cppFiles += ../../zen/zstring.cpp
cppFiles += ../../zen/format_unit.cpp
cppFiles += ../../zen/legacy_compiler.cpp
cppFiles += ../../zen/open_ssl.cpp
cppFiles += ../../zen/process_priority.cpp
cppFiles += ../../zen/recycler.cpp
cppFiles += ../../zen/resolve_path.cpp
cppFiles += ../../zen/process_exec.cpp
cppFiles += ../../zen/shutdown.cpp
cppFiles += ../../zen/sys_error.cpp
cppFiles += ../../zen/sys_info.cpp
cppFiles += ../../zen/sys_version.cpp
cppFiles += ../../zen/thread.cpp
cppFiles += ../../zen/zlib_wrap.cpp
cppFiles += ../../wx+/darkmode.cpp
cppFiles += ../../wx+/file_drop.cpp
cppFiles += ../../wx+/grid.cpp
cppFiles += ../../wx+/image_tools.cpp
cppFiles += ../../wx+/graph.cpp
cppFiles += ../../wx+/taskbar.cpp
cppFiles += ../../wx+/tooltip.cpp
cppFiles += ../../wx+/image_resources.cpp
cppFiles += ../../wx+/popup_dlg.cpp
cppFiles += ../../wx+/popup_dlg_generated.cpp
cppFiles += ../../xBRZ/src/xbrz.cpp

tmpPath := $(shell dirname "$(shell mktemp -u)")/$(exeName)_Make

objFiles := $(cppFiles:%=$(tmpPath)/ffs/src/%.o)

all: ../Build/Bin/$(exeName)

../Build/Bin/$(exeName): $(objFiles)
	mkdir -p $(dir $@)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(tmpPath)/ffs/src/%.o : %
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(tmpPath)
	rm -f ../Build/Bin/$(exeName)
