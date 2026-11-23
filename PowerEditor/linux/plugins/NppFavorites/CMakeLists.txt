# NppFavorites (https://github.com/heldersepu/nppfavorites).
# Source revision 6f92721f92c6193a78f6fcf5b17fbeb73445b749.
npp_add_plugin(NppFavorites DLLMAIN
	NppFavorites.cpp PluginDefinition.cpp DockingFeature/StaticDialog.cpp
	RC version_info.rc)
target_include_directories(NppFavorites PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/DockingFeature")
install(FILES COPYING DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/NppFavorites")
