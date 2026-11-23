# DSpellCheck (https://github.com/Predelnik/DSpellCheck), GPL v2 (License.txt); Hunspell MPL 1.1/GPL 2/LGPL 2.1
# (deps/Hunspell/COPYING*), zlib/minizip (deps/minizip).
# Source revision a152beb134d091771829e11dd53a4b7a766fbef6. The sources of its CMakeLists.txt, without the Windows
# speller (ISpellChecker), win-iconv (iconv of glibc), the gz* files of zlib and the tests. FtpClient uses its Unix sockets.
# deps/Hunspell/src/hunspell/utf_info.cxx is included by csutil.cxx.
set(DSPELLCHECK_SOURCES
	src/common/IniWorker.cpp src/common/PrecompiledHeader.cpp src/common/ProgressData.cpp
	src/common/TaskWrapper.cpp src/common/Utility.cpp src/common/Win32Exception.cpp src/common/WinApiControls.cpp
	src/common/WinInet.cpp src/common/raii.cpp src/common/string_utils.cpp src/common/utf8.cpp
	src/common/winapi.cpp src/core/SpellChecker.cpp src/core/SpellCheckerHelpers.cpp
	src/network/GithubFileListProvider.cpp src/network/UrlHelpers.cpp src/npp/EditorInterface.cpp
	src/npp/NppInterface.cpp src/npp/ScintillaUtils.cpp src/npp/TextUtils.cpp src/plugin/DllMain.cpp
	src/plugin/Plugin.cpp src/plugin/Settings.cpp src/spellers/AspellInterface.cpp
	src/spellers/HunspellInterface.cpp src/spellers/NativeSpellerInterface.cpp src/spellers/SpellerContainer.cpp
	src/spellers/SpellerInterface.cpp src/spellers/aspell.cpp src/ui/AboutDialog.cpp
	src/ui/AspellOptionsDialog.cpp src/ui/ConnectionSettingsDialog.cpp src/ui/ContextMenuHandler.cpp
	src/ui/DownloadDictionariesDialog.cpp src/ui/MenuItem.cpp src/ui/ProgressDialog.cpp
	src/ui/RemoveDictionariesDialog.cpp src/ui/SelectMultipleLanguagesDialog.cpp src/ui/SettingsDialog.cpp
	src/ui/StaticDialog.cpp src/ui/SuggestionMenuButton.cpp)
set(DSPELLCHECK_DEPS
	deps/Controls/CheckedList/CheckedList.cpp deps/Controls/TabBar/ControlsTab.cpp deps/Controls/TabBar/TabBar.cpp
	deps/Hunspell/src/hunspell/affentry.cxx deps/Hunspell/src/hunspell/affixmgr.cxx
	deps/Hunspell/src/hunspell/csutil.cxx deps/Hunspell/src/hunspell/filemgr.cxx
	deps/Hunspell/src/hunspell/hashmgr.cxx deps/Hunspell/src/hunspell/hunspell.cxx
	deps/Hunspell/src/hunspell/hunzip.cxx deps/Hunspell/src/hunspell/phonet.cxx
	deps/Hunspell/src/hunspell/replist.cxx deps/Hunspell/src/hunspell/suggestmgr.cxx deps/aspell/dummy.cpp
	deps/minizip/adler32.c deps/minizip/compress.c deps/minizip/crc32.c deps/minizip/deflate.c
	
	deps/minizip/infback.c deps/minizip/inffast.c deps/minizip/inflate.c deps/minizip/inftrees.c
	deps/minizip/ioapi.c deps/minizip/trees.c deps/minizip/uncompr.c deps/minizip/unzip.c deps/minizip/zutil.c
	deps/npp_extra/dummy.cpp
	deps/FtpClient/BlockingSocket.cpp deps/FtpClient/FTPClient.cpp deps/FtpClient/FTPDataTypes.cpp deps/FtpClient/FTPFileStatus.cpp
	deps/FtpClient/FTPListParse.cpp)
npp_add_plugin(DSpellCheck DLLMAIN ${DSPELLCHECK_SOURCES} ${DSPELLCHECK_DEPS}
	RC src/plugin/DSpellCheck.rc)
target_include_directories(DSpellCheck PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/deps/Hunspell/src"
	"${CMAKE_CURRENT_SOURCE_DIR}/deps/Controls"
	"${CMAKE_CURRENT_SOURCE_DIR}/deps/npp_extra"
	"${CMAKE_CURRENT_SOURCE_DIR}/deps/lsignal"
	"${CMAKE_CURRENT_SOURCE_DIR}/deps/aspell"
	"${CMAKE_CURRENT_SOURCE_DIR}/deps/json"
	"${CMAKE_CURRENT_SOURCE_DIR}/deps/minizip"
	"${CMAKE_CURRENT_SOURCE_DIR}/deps/FtpClient")
target_compile_definitions(DSpellCheck PRIVATE HUNSPELL_STATIC NOMINMAX)
# the precompiled header of the Windows build: the sources of the plugin expect it
set_source_files_properties(${DSPELLCHECK_SOURCES} PROPERTIES
	COMPILE_FLAGS "-include ${CMAKE_CURRENT_SOURCE_DIR}/src/common/PrecompiledHeader.h")
install(FILES License.txt DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/DSpellCheck")
