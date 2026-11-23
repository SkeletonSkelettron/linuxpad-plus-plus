# QuickText (https://github.com/vinsworldcom/nppQuickText), GPL v2 or later.
# Source revision fc21782d3b8d7553b5df4ce7936758af489fadd4.
npp_add_plugin(QuickText DLLMAIN
	QuickText.cpp lib/INIMap.cpp
	RC QuickText.rc)
target_include_directories(QuickText PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/lib")
# the default snippets next to the plugin (copied to the plugins configuration folder on first use)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Config/QuickText.default.ini" "${CMAKE_BINARY_DIR}/lib/linuxpad++/plugins/QuickText/QuickText.default.ini" COPYONLY)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/Config/QuickText.default.ini" DESTINATION "lib/linuxpad++/plugins/QuickText")
