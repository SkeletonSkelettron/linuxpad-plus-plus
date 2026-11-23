# NppRestoreCursorPosition (https://github.com/RivieraSystems/NppRestoreCursorPosition), MIT.
# Source revision 4f84cf3ad4c6b588d268632dfe1cdb96b4e588f5.
npp_add_plugin(NppRestoreCursorPosition DLLMAIN
	src/HistoryStore.cpp src/NppRestoreCursorPosition.cpp src/PluginDefinition.cpp src/UiText.cpp
	RC NppRestoreCursorPosition.rc)
target_include_directories(NppRestoreCursorPosition PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/NppRestoreCursorPosition")
