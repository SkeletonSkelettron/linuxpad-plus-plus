# XBrackets Lite (https://github.com/d0vgan/npp-XBracketsLite) by DV.
# Source revision 76b12358e1f2cad082830b429e13033af61509b4.
npp_add_plugin(XBrackets DLLMAIN
	Main.cpp CFileModificationWatcher.cpp SettingsDlg.cpp XBracketsCommon.cpp XBracketsLogic.cpp
	XBracketsMenu.cpp XBracketsOptions.cpp XBracketsPlugin.cpp
	core/NppMessager.cpp core/NppPlugin.cpp core/NppPluginMenu.cpp core/SciMessager.cpp
	json11/json11.cpp
	RC XBrackets.rc)
target_include_directories(XBrackets PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/core" "${CMAKE_CURRENT_SOURCE_DIR}/json11")
# the default configuration and the help next to the plugin
foreach(_file XBrackets_Config.json XBrackets.txt)
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${_file}" "${CMAKE_BINARY_DIR}/lib/linuxpad++/plugins/XBrackets/${_file}" COPYONLY)
	install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${_file}" DESTINATION "lib/linuxpad++/plugins/XBrackets")
endforeach()
install(FILES COPYING DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/XBrackets")
