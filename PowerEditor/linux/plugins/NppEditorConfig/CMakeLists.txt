# Qt port of editorconfig/editorconfig-notepad-plus-plus at 136337da8c028a8bedec07285622b154ef1896c1.
# editorconfig-core-c v0.12.11, revision 0ba157eff167c1b2d1bc0c3e13975b7a73fb9d25.
find_package(PkgConfig REQUIRED)
pkg_check_modules(EDITORCONFIG_PCRE2 REQUIRED IMPORTED_TARGET libpcre2-8)
npp_add_plugin(NppEditorConfig EditorConfig.cpp
	vendor/core/src/ec_glob.c vendor/core/src/editorconfig.c
	vendor/core/src/editorconfig_handle.c vendor/core/src/ini.c vendor/core/src/misc.c)
target_include_directories(NppEditorConfig PRIVATE vendor/core/include vendor/core)
target_compile_options(NppEditorConfig PRIVATE $<$<COMPILE_LANGUAGE:C>:-funsigned-char>)
target_link_libraries(NppEditorConfig PRIVATE PkgConfig::EDITORCONFIG_PCRE2)
install(FILES COPYING DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/NppEditorConfig")
install(FILES vendor/core/LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/NppEditorConfig/core")
