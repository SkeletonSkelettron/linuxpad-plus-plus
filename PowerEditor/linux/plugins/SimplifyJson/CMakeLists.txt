# SimplifyJson (https://github.com/analystvipul/SimplifyJson).
# Source revision 231df4c7b8f506c086cceb74522f716cc220f712.
npp_add_plugin(SimplifyJson DLLMAIN
	src/JsonUtils.cpp src/NppJsonTools.cpp src/PluginDefinition.cpp
	RC src/NppJsonTools.rc)
target_include_directories(SimplifyJson PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
