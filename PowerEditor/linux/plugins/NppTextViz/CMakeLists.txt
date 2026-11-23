# NppTextViz (https://github.com/chcg/NppTextViz), GPL v2 or later.
# Source revision 200a48ef8fb3838afc838631fd81e97a15fede15.
npp_add_plugin(NppTextViz DLLMAIN
	src/NppTextViz.cpp src/PluginDefinition.cpp src/DockingFeature/StaticDialog.cpp src/DockingFeature/sequenceBoxDlg.cpp
	RC src/NppTextViz.rc src/DockingFeature/sequenceBox.rc)
target_include_directories(NppTextViz PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/DockingFeature")
