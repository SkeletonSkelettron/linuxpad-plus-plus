# This file is part of Notepad++ project
# Copyright (C)2026 Notepad++ Linux port contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# at your option any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# LinuxPad++: the native Linux (Qt 6) build of Notepad++.
#
#   cmake -S PowerEditor/linux -B build -G Ninja
#   cmake --build build
#   ./build/bin/linuxpad++
#
# The Windows sources under scintilla/, lexilla/ and boostregex/ are used
# as they are. The Win32 user interface of PowerEditor/src is replaced by the
# Qt code in PowerEditor/linux/src; the portable parts of PowerEditor/src
# (encoding mapper, uchardet, pugixml, menu command ids, icons, XML data)
# are reused directly.

cmake_minimum_required(VERSION 3.21)

# Version of the Windows build (PowerEditor/src/resource.h)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../src/resource.h" _npp_version_line REGEX "define VERSION_PRODUCT_VALUE")
string(REGEX MATCH "[0-9]+(\\.[0-9]+)+" NPP_VERSION "${_npp_version_line}")
if(NOT NPP_VERSION)
	set(NPP_VERSION "0.0.0")
endif()
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)?(\\.[0-9]+)?" _npp_cmake_version "${NPP_VERSION}")

# Version of LinuxPad++ (its own releases); NPP_VERSION stays the Notepad++ version it is based on, the one the
# plugins get (NPPM_GETNPPVERSION)
set(LINUXPAD_VERSION "0.0.1")

project(LinuxPadPlusPlus VERSION ${LINUXPAD_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
	message(FATAL_ERROR "PowerEditor/linux is the Linux front end. Use PowerEditor/visual.net or PowerEditor/gcc on Windows.")
endif()

include(GNUInstallDirs)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network Core5Compat PrintSupport)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)

set(NPP_ROOT      "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(NPP_SRC       "${NPP_ROOT}/PowerEditor/src")
set(NPP_INSTALLER "${NPP_ROOT}/PowerEditor/installer")
set(SCI_DIR       "${NPP_ROOT}/scintilla")
set(LEX_DIR       "${NPP_ROOT}/lexilla")
set(BOOST_DIR     "${NPP_ROOT}/boostregex")

# Versions of the bundled libraries, shown in "? > Debug Info" (NppLibsVersionH-generator.bat on Windows)
file(READ "${SCI_DIR}/version.txt" _sci_ver)
string(STRIP "${_sci_ver}" _sci_ver)
string(REGEX REPLACE "^([0-9])([0-9])([0-9]+)$" "\\1.\\2.\\3" NPP_SCINTILLA_VERSION "${_sci_ver}")
file(READ "${LEX_DIR}/version.txt" _lex_ver)
string(STRIP "${_lex_ver}" _lex_ver)
string(REGEX REPLACE "^([0-9])([0-9])([0-9]+)$" "\\1.\\2.\\3" NPP_LEXILLA_VERSION "${_lex_ver}")
file(STRINGS "${BOOST_DIR}/boost/version.hpp" _boost_line REGEX "define BOOST_LIB_VERSION")
string(REGEX MATCH "\"[0-9_]+\"" NPP_BOOST_VERSION "${_boost_line}")
string(REPLACE "\"" "" NPP_BOOST_VERSION "${NPP_BOOST_VERSION}")

# ---------------------------------------------------------------------------
# Scintilla, Qt platform layer, with Notepad++'s boost regex search engine
# ---------------------------------------------------------------------------
file(GLOB SCI_CORE_SOURCES CONFIGURE_DEPENDS "${SCI_DIR}/src/*.cxx")

add_library(npp_scintilla STATIC
	${SCI_CORE_SOURCES}
	"${SCI_DIR}/qt/ScintillaEditBase/PlatQt.cpp"
	"${SCI_DIR}/qt/ScintillaEditBase/ScintillaQt.cpp"
	"${SCI_DIR}/qt/ScintillaEditBase/ScintillaQt.h"
	"${SCI_DIR}/qt/ScintillaEditBase/ScintillaEditBase.cpp"
	"${SCI_DIR}/qt/ScintillaEditBase/ScintillaEditBase.h"
	"${BOOST_DIR}/BoostRegExSearch.cxx"
	"${BOOST_DIR}/UTF8DocumentIterator.cxx"
)
set_target_properties(npp_scintilla PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
target_include_directories(npp_scintilla
	PUBLIC  "${SCI_DIR}/include" "${SCI_DIR}/qt/ScintillaEditBase" "${SCI_DIR}/src"
	PRIVATE "${BOOST_DIR}")
target_compile_definitions(npp_scintilla
	PUBLIC  SCINTILLA_QT=1
	PRIVATE MAKING_LIBRARY=1 SCI_OWNREGEX BOOST_REGEX_STANDALONE)
target_compile_options(npp_scintilla PRIVATE -w -include cstdint)
target_link_libraries(npp_scintilla PUBLIC Qt6::Widgets Qt6::Core5Compat)

# ---------------------------------------------------------------------------
# Lexilla (static, includes Notepad++'s UDL and search result lexers)
# ---------------------------------------------------------------------------
file(GLOB LEX_SOURCES CONFIGURE_DEPENDS
	"${LEX_DIR}/src/Lexilla.cxx"
	"${LEX_DIR}/lexlib/*.cxx"
	"${LEX_DIR}/lexers/*.cxx")

add_library(npp_lexilla STATIC ${LEX_SOURCES})
set_target_properties(npp_lexilla PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(npp_lexilla
	PUBLIC  "${LEX_DIR}/include"
	PRIVATE "${LEX_DIR}/lexlib" "${SCI_DIR}/include")
target_compile_options(npp_lexilla PRIVATE -w -include cstdint)
# LexUser.cxx (Notepad++ UDL lexer) includes <windows.h> for _itoa only.
set_source_files_properties("${LEX_DIR}/lexers/LexUser.cxx" PROPERTIES
	INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/compat")

# ---------------------------------------------------------------------------
# Portable parts of PowerEditor/src, compiled unchanged
# ---------------------------------------------------------------------------
file(GLOB UCHARDET_SOURCES CONFIGURE_DEPENDS "${NPP_SRC}/uchardet/*.cpp")

add_library(npp_portable STATIC
	${UCHARDET_SOURCES}
	"${NPP_SRC}/EncodingMapper.cpp"
	"${NPP_SRC}/pugixml/pugixml.cpp")
target_include_directories(npp_portable PUBLIC
	"${NPP_SRC}" "${NPP_SRC}/uchardet" "${NPP_SRC}/pugixml" "${SCI_DIR}/include")
target_compile_options(npp_portable PRIVATE -w -include "${CMAKE_CURRENT_SOURCE_DIR}/compat/npp_posix.h")

# ---------------------------------------------------------------------------
# Tables generated from the Windows sources (menus, shortcuts, languages)
# ---------------------------------------------------------------------------
set(NPP_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(NPP_GEN_FILES
	"${NPP_GEN_DIR}/LangType.h"
	"${NPP_GEN_DIR}/LangTable.inc"
	"${NPP_GEN_DIR}/MenuTable.inc"
	"${NPP_GEN_DIR}/ShortcutTable.inc"
	"${NPP_GEN_DIR}/ContextMenuDefault.inc"
	"${NPP_GEN_DIR}/ScintillaKeyTable.inc")
add_custom_command(
	OUTPUT ${NPP_GEN_FILES}
	COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_tables.py"
		"${NPP_SRC}" "${NPP_INSTALLER}" "${NPP_GEN_DIR}"
	DEPENDS
		"${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_tables.py"
		"${NPP_SRC}/Notepad_plus.rc"
		"${NPP_SRC}/Parameters.cpp"
		"${NPP_SRC}/localization.cpp"
		"${NPP_SRC}/Notepad_plus.cpp"
		"${NPP_SRC}/ScintillaComponent/ScintillaEditView.cpp"
		"${NPP_SRC}/MISC/PluginsManager/Notepad_plus_msgs.h"
		"${NPP_SRC}/MISC/Common/NppConstants.h"
		"${NPP_INSTALLER}/nativeLang/english.xml"
	COMMENT "Generating menu, shortcut and language tables from the Windows sources"
	VERBATIM)

# ---------------------------------------------------------------------------
# linuxpad++ executable
# ---------------------------------------------------------------------------
file(GLOB NPP_LINUX_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
list(FILTER NPP_LINUX_SOURCES EXCLUDE REGEX "/src/main\\.cpp$")

# everything but main(), shared by the executable and the self test
add_library(npp_app STATIC ${NPP_LINUX_SOURCES} ${NPP_GEN_FILES})
set_target_properties(npp_app PROPERTIES AUTOMOC ON)
target_include_directories(npp_app PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/src" "${NPP_GEN_DIR}" "${NPP_SRC}" "${SCI_DIR}/include" "${LEX_DIR}/include")
target_compile_definitions(npp_app PUBLIC
	NPP_VERSION_STRING="${NPP_VERSION}"
	LINUXPAD_VERSION_STRING="${LINUXPAD_VERSION}"
	NPP_SCINTILLA_VERSION="${NPP_SCINTILLA_VERSION}"
	NPP_LEXILLA_VERSION="${NPP_LEXILLA_VERSION}"
	NPP_BOOST_VERSION="${NPP_BOOST_VERSION}")
target_compile_options(npp_app PUBLIC -include cstdint PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(npp_app PUBLIC
	npp_scintilla npp_lexilla npp_portable
	Qt6::Widgets Qt6::Network Qt6::PrintSupport)

# the plugin interface of Notepad++ (plugin SDK) for the plugins manager
set(NPP_PLUGIN_SDK_INCLUDES
	"${CMAKE_CURRENT_SOURCE_DIR}/plugin-sdk/include"
	"${NPP_SRC}/MISC/PluginsManager"
	"${NPP_SRC}/WinControls/DockingWnd")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/src/PluginsManager.cpp"
	PROPERTIES INCLUDE_DIRECTORIES "${NPP_PLUGIN_SDK_INCLUDES}")

add_executable(linuxpad++ "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
set_target_properties(linuxpad++ PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_options(linuxpad++ PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(linuxpad++ PRIVATE npp_app)

# Icons of the Windows build, compiled into the executable
file(GLOB_RECURSE NPP_ICON_FILES RELATIVE "${NPP_SRC}" CONFIGURE_DEPENDS
	"${NPP_SRC}/icons/light/toolbar/regular/*.ico"
	"${NPP_SRC}/icons/light/toolbar/filled/*.ico"
	"${NPP_SRC}/icons/dark/toolbar/regular/*.ico"
	"${NPP_SRC}/icons/dark/toolbar/filled/*.ico"
	"${NPP_SRC}/icons/standard/toolbar/*.bmp"
	"${NPP_SRC}/icons/standard/tabbar/*.ico"
	"${NPP_SRC}/icons/dark/tabbar/*.ico"
	"${NPP_SRC}/icons/standard/panels/*/*.ico"
	"${NPP_SRC}/icons/dark/panels/*/*.ico"
	"${NPP_SRC}/icons/npp.ico")
set(NPP_ICON_ABS)
foreach(f ${NPP_ICON_FILES})
	list(APPEND NPP_ICON_ABS "${NPP_SRC}/${f}")
endforeach()
qt_add_resources(linuxpad++ "npp_icons" PREFIX "/" BASE "${NPP_SRC}" FILES ${NPP_ICON_ABS})
# the logo of LinuxPad++ (window icon, About), :/linuxpad/linuxpad-plus-plus-<size>.png
set(LINUXPAD_ICONS)
foreach(_size 16 24 32 48 64 128 256)
	list(APPEND LINUXPAD_ICONS "${CMAKE_CURRENT_SOURCE_DIR}/data/icons/linuxpad-plus-plus-${_size}.png")
endforeach()
qt_add_resources(linuxpad++ "linuxpad_icons" PREFIX "/linuxpad" BASE "${CMAKE_CURRENT_SOURCE_DIR}/data/icons" FILES ${LINUXPAD_ICONS})

option(NPP_BUILD_PLUGINS "Build the plugins ported to Linux (plugins/)" ON)

# Self test: drives the main window with the offscreen platform (not installed)
option(NPP_BUILD_TESTS "Build the self test (tests/selftest.cpp)" OFF)
if(NPP_BUILD_TESTS)
	add_executable(npp_selftest "${CMAKE_CURRENT_SOURCE_DIR}/tests/selftest.cpp")
	set_target_properties(npp_selftest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
	target_link_libraries(npp_selftest PRIVATE npp_app npp_win32)
	target_compile_definitions(npp_selftest PRIVATE NPP_TEST_BUNDLED_PLUGINS=$<BOOL:${NPP_BUILD_PLUGINS}>)
	qt_add_resources(npp_selftest "npp_icons_test" PREFIX "/" BASE "${NPP_SRC}" FILES ${NPP_ICON_ABS})
	qt_add_resources(npp_selftest "linuxpad_icons_test" PREFIX "/linuxpad" BASE "${CMAKE_CURRENT_SOURCE_DIR}/data/icons" FILES ${LINUXPAD_ICONS})
	add_dependencies(npp_selftest npp_data)
	enable_testing()
	add_test(NAME selftest COMMAND npp_selftest)
	add_test(NAME light_mode_on_dark_desktop COMMAND npp_selftest --test-light-mode)

	if(NPP_BUILD_PLUGINS)
		# Exercise the bundled Linux plugins together in an isolated profile.
		add_executable(npp_plugintest "${CMAKE_CURRENT_SOURCE_DIR}/tests/plugintest.cpp")
		set_target_properties(npp_plugintest PROPERTIES AUTOMOC ON RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
		target_link_libraries(npp_plugintest PRIVATE npp_app)
		qt_add_resources(npp_plugintest "npp_icons_plugintest" PREFIX "/" BASE "${NPP_SRC}" FILES ${NPP_ICON_ABS})
		qt_add_resources(npp_plugintest "linuxpad_icons_plugintest" PREFIX "/linuxpad" BASE "${CMAKE_CURRENT_SOURCE_DIR}/data/icons" FILES ${LINUXPAD_ICONS})
		add_dependencies(npp_plugintest npp_data)
		add_test(NAME plugintest COMMAND npp_plugintest)
		set_tests_properties(plugintest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 900)
	endif()
	set_tests_properties(selftest light_mode_on_dark_desktop PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
	set_tests_properties(light_mode_on_dark_desktop PROPERTIES TIMEOUT 20)

	# plugin of the self test, not installed: <build>/test-plugins/NppTestPlugin/NppTestPlugin.so
	add_library(NppTestPlugin MODULE "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestPlugin.cpp")
	set_target_properties(NppTestPlugin PROPERTIES PREFIX "" AUTOMOC ON CXX_VISIBILITY_PRESET hidden
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test-plugins/NppTestPlugin")
	target_link_libraries(NppTestPlugin PRIVATE npp_plugin_sdk npp_win32)
	add_dependencies(npp_selftest NppTestPlugin)
	target_include_directories(npp_selftest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
	set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/tests/selftest.cpp"
		PROPERTIES INCLUDE_DIRECTORIES "${NPP_PLUGIN_SDK_INCLUDES}")

	# Report of the menu commands that are not implemented yet (see PORTING_STATUS.md)
	add_executable(npp_menu_coverage "${CMAKE_CURRENT_SOURCE_DIR}/tests/menu_coverage.cpp")
	set_target_properties(npp_menu_coverage PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
	target_link_libraries(npp_menu_coverage PRIVATE npp_app)
	add_dependencies(npp_menu_coverage npp_data)
endif()

# ---------------------------------------------------------------------------
# Data files: <build>/share/linuxpad++ mirrors <prefix>/share/linuxpad++
# ---------------------------------------------------------------------------
set(NPP_DATA_OUT "${CMAKE_BINARY_DIR}/share/linuxpad++")
add_custom_target(npp_data ALL
	COMMAND ${CMAKE_COMMAND} -E make_directory "${NPP_DATA_OUT}"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different "${NPP_SRC}/langs.model.xml" "${NPP_SRC}/stylers.model.xml" "${NPP_DATA_OUT}/"
	COMMAND ${CMAKE_COMMAND} -E copy_directory "${NPP_INSTALLER}/themes" "${NPP_DATA_OUT}/themes"
	COMMAND ${CMAKE_COMMAND} -E copy_directory "${NPP_INSTALLER}/nativeLang" "${NPP_DATA_OUT}/localization"
	COMMAND ${CMAKE_COMMAND} -E copy_directory "${NPP_INSTALLER}/APIs" "${NPP_DATA_OUT}/autoCompletion"
	COMMAND ${CMAKE_COMMAND} -E copy_directory "${NPP_INSTALLER}/functionList" "${NPP_DATA_OUT}/functionList"
	COMMENT "Copying Notepad++ data files")
add_dependencies(linuxpad++ npp_data)

# ---------------------------------------------------------------------------
# Plugins: built with the Linux plugin SDK, <build>/lib/linuxpad++/plugins/<Name>/<Name>.so
# mirrors <prefix>/lib/linuxpad++/plugins
# ---------------------------------------------------------------------------
set(NPP_PLUGIN_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/plugin-sdk")
add_library(npp_plugin_sdk INTERFACE)
target_include_directories(npp_plugin_sdk INTERFACE
	${NPP_PLUGIN_SDK_INCLUDES} "${SCI_DIR}/include" "${LEX_DIR}/include")
target_link_libraries(npp_plugin_sdk INTERFACE Qt6::Widgets)
# plugins can test it where they choose their Windows code with _WIN32 (which Qt must not see)
target_compile_definitions(npp_plugin_sdk INTERFACE NPP_PLUGIN_SDK_LINUX=1)
target_compile_options(npp_plugin_sdk INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-include npp_win32/w32_crt.h>) # <cstdint>, CRT names, __int64

# Each plugin gets its own compatibility state, including its registered resources.
add_library(npp_win32 STATIC EXCLUDE_FROM_ALL
	"${NPP_PLUGIN_SDK_DIR}/win32/CommonControls.cpp"
	"${NPP_PLUGIN_SDK_DIR}/win32/Crt.cpp"
	"${NPP_PLUGIN_SDK_DIR}/win32/Dialogs.cpp"
	"${NPP_PLUGIN_SDK_DIR}/win32/FileDialog.cpp"
	"${NPP_PLUGIN_SDK_DIR}/win32/Gdi.cpp"
	"${NPP_PLUGIN_SDK_DIR}/win32/Kernel.cpp"
	"${NPP_PLUGIN_SDK_DIR}/win32/Menus.cpp"
	"${NPP_PLUGIN_SDK_DIR}/win32/Ole.cpp"
	"${NPP_PLUGIN_SDK_DIR}/win32/Resources.cpp"
	"${NPP_PLUGIN_SDK_DIR}/win32/RichEdit.cpp"
	"${NPP_PLUGIN_SDK_DIR}/win32/Toolbar.cpp"
	"${NPP_PLUGIN_SDK_DIR}/win32/Shell.cpp"
	"${NPP_PLUGIN_SDK_DIR}/win32/TaskDialog.cpp"
	"${NPP_PLUGIN_SDK_DIR}/win32/WinInet.cpp"
	"${NPP_PLUGIN_SDK_DIR}/win32/Window.cpp")
set_target_properties(npp_win32 PROPERTIES
	POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
target_link_libraries(npp_win32 PRIVATE npp_plugin_sdk Qt6::Network Threads::Threads ${CMAKE_DL_LIBS})

# Lexilla's lexlib for lexer plugins (DefaultLexer, StyleContext, WordList...): a copy of their own
file(GLOB NPP_LEXLIB_SOURCES CONFIGURE_DEPENDS "${LEX_DIR}/lexlib/*.cxx")
add_library(npp_plugin_lexlib STATIC EXCLUDE_FROM_ALL ${NPP_LEXLIB_SOURCES})
set_target_properties(npp_plugin_lexlib PROPERTIES
	POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
target_include_directories(npp_plugin_lexlib PUBLIC "${LEX_DIR}/lexlib" "${LEX_DIR}/include" "${SCI_DIR}/include")
target_compile_options(npp_plugin_lexlib PRIVATE -w -include cstdint)

# npp_add_plugin(<Name> [DLLMAIN] [EXPORT_ALL] [LEXLIB] <sources>... [RC <resources>...])
# DLLMAIN calls the plugin's DllMain with DLL_PROCESS_ATTACH at load time.
# EXPORT_ALL: all symbols visible, for modules whose exports a .def file lists on Windows.
# LEXLIB: links Lexilla's lexlib (lexer plugins built on DefaultLexer, StyleContext...).
function(npp_add_plugin name)
	cmake_parse_arguments(PARSE_ARGV 1 NPP_PLUGIN "DLLMAIN;EXPORT_ALL;LEXLIB" "" "RC;RC_INCLUDES")
	if(NPP_PLUGIN_KEYWORDS_MISSING_VALUES)
		message(FATAL_ERROR "${name}: RC requires at least one resource file")
	endif()
	set(_sources ${NPP_PLUGIN_UNPARSED_ARGUMENTS})
	if(NPP_PLUGIN_DLLMAIN)
		list(APPEND _sources "${NPP_PLUGIN_SDK_DIR}/src/DllMainAttach.cpp")
	endif()
	# the folder of the plugin and RC_INCLUDES folders are include paths of the .rc files
	set(_rc_includes --include "${NPP_PLUGIN_SDK_DIR}/include" --include "${CMAKE_CURRENT_SOURCE_DIR}")
	foreach(_dir IN LISTS NPP_PLUGIN_RC_INCLUDES)
		get_filename_component(_dir "${_dir}" ABSOLUTE)
		list(APPEND _rc_includes --include "${_dir}")
	endforeach()
	set(_resource_index 0)
	foreach(_resource IN LISTS NPP_PLUGIN_RC)
		get_filename_component(_input "${_resource}" ABSOLUTE)
		math(EXPR _resource_index "${_resource_index} + 1")
		set(_output "${CMAKE_CURRENT_BINARY_DIR}/${name}_rc_${_resource_index}.cpp")
		set(_compiler "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/rc2cpp.py")
		add_custom_command(
			OUTPUT "${_output}"
			COMMAND Python3::Interpreter "${_compiler}"
				--compiler "${CMAKE_C_COMPILER}" ${_rc_includes}
				--depfile "${_output}.d" "${_input}" "${_output}"
			DEPENDS "${_input}" "${_compiler}"
			DEPFILE "${_output}.d"
			COMMENT "Compiling ${name} resource ${_resource}"
			VERBATIM)
		list(APPEND _sources "${_output}")
	endforeach()
	add_library(${name} MODULE ${_sources})
	set_target_properties(${name} PROPERTIES
		PREFIX ""
		AUTOMOC ON
		CXX_VISIBILITY_PRESET hidden
		C_VISIBILITY_PRESET hidden
		VISIBILITY_INLINES_HIDDEN ON
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/linuxpad++/plugins/${name}")
	if(NPP_PLUGIN_EXPORT_ALL)
		set_target_properties(${name} PROPERTIES CXX_VISIBILITY_PRESET default C_VISIBILITY_PRESET default VISIBILITY_INLINES_HIDDEN OFF)
	endif()
	target_link_libraries(${name} PRIVATE npp_plugin_sdk npp_win32)
	if(NPP_PLUGIN_LEXLIB)
		target_link_libraries(${name} PRIVATE npp_plugin_lexlib)
	endif()
	set_property(GLOBAL APPEND PROPERTY NPP_PLUGIN_TARGETS ${name})
	target_link_options(${name} PRIVATE -Wl,-Bsymbolic -Wl,--no-undefined)
	install(TARGETS ${name} LIBRARY DESTINATION "lib/linuxpad++/plugins/${name}") # <exe>/../lib: see PluginsManager::pluginsDirs()
endfunction()

if(NPP_BUILD_PLUGINS)
	add_subdirectory(plugins)
	if(NPP_BUILD_TESTS)
		add_dependencies(npp_selftest mimeTools NppConverter nppAutoDetectIndent DiffChecker)
		get_property(_npp_plugin_targets GLOBAL PROPERTY NPP_PLUGIN_TARGETS)
		add_dependencies(npp_plugintest ${_npp_plugin_targets})
	endif()
endif()

if(NPP_BUILD_TESTS)
	target_compile_definitions(npp_selftest PRIVATE NPP_TEST_JSONTOOLS=$<TARGET_EXISTS:JsonTools>)
	if(TARGET JsonTools)
		add_dependencies(npp_selftest JsonTools)
	endif()
endif()

install(TARGETS linuxpad++ RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY "${NPP_DATA_OUT}/" DESTINATION "${CMAKE_INSTALL_DATADIR}/linuxpad++")
# Desktop entry with absolute paths: ~/.local/bin is not always in the PATH of the
# desktop session and ~/.local/share/pixmaps is not an icon search path.
# Choose the prefix at configure time (-DCMAKE_INSTALL_PREFIX=...).
set(NPP_INSTALL_BINARY "${CMAKE_INSTALL_FULL_BINDIR}/linuxpad++")
set(NPP_INSTALL_ICON "${CMAKE_INSTALL_FULL_DATADIR}/linuxpad++/linuxpad-plus-plus.png")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/data/linuxpad-plus-plus.desktop.in"
	"${CMAKE_BINARY_DIR}/linuxpad-plus-plus.desktop" @ONLY)
install(FILES "${CMAKE_BINARY_DIR}/linuxpad-plus-plus.desktop"
	DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/icons/linuxpad-plus-plus-256.png"
	DESTINATION "${CMAKE_INSTALL_DATADIR}/linuxpad++" RENAME linuxpad-plus-plus.png)
foreach(_size 16 24 32 48 64 128 256 512)
	install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/icons/linuxpad-plus-plus-${_size}.png"
		DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_size}x${_size}/apps" RENAME linuxpad-plus-plus.png)
endforeach()
