# NppPluginDemo (https://github.com/npp-plugins/plugindemo).
# Source revision 030d76485ebc5d3e94cbb2019472319db72ad9ae.
npp_add_plugin(NppPluginDemo DLLMAIN
	src/NppPluginDemo.cpp src/PluginDefinition.cpp src/DockingFeature/GoToLineDlg.cpp src/DockingFeature/StaticDialog.cpp
	RC src/NppPluginDemo.rc src/DockingFeature/goLine.rc)
target_include_directories(NppPluginDemo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/DockingFeature")
