# NppZenHankaku (https://github.com/RivieraSystems/NppZenHankaku), MIT.
# Source revision ca56e00034a2427c329651c67aff75375768d90b.
npp_add_plugin(NppZenHankaku DLLMAIN
	src/DbcsCodec.cpp src/NppZenHankaku.cpp src/PluginDefinition.cpp src/ToolbarIcons.cpp src/converter.cpp
	RC NppZenHankaku.rc)
target_include_directories(NppZenHankaku PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/NppZenHankaku")
