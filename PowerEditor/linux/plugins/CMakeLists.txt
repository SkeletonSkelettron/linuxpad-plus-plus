# Plugins of Notepad++ ported to Linux: one folder each, built with npp_add_plugin()
# (see ../CMakeLists.txt and ../plugin-sdk). PLUGINS_STATUS.md lists them.
file(GLOB _npp_plugin_dirs LIST_DIRECTORIES true CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*")
list(SORT _npp_plugin_dirs)
foreach(_dir ${_npp_plugin_dirs})
	if(IS_DIRECTORY "${_dir}" AND EXISTS "${_dir}/CMakeLists.txt")
		add_subdirectory("${_dir}")
	endif()
endforeach()
