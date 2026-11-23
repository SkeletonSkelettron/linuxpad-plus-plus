# NppStripIpAndHost (https://github.com/larreblarsson/NppStripIpAndHost), GPL v2 or later.
# Source revision 589e6f53f3c934f1acc949bd701855b0b33527f9.
npp_add_plugin(NppStripIpAndHost DLLMAIN
	src/NppPluginDemo.cpp src/PluginDefinition.cpp
	RC src/NppPluginDemo.rc)
target_include_directories(NppStripIpAndHost PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(NppStripIpAndHost PRIVATE Qt6::Network)
install(FILES LICENSE.txt DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/NppStripIpAndHost")
