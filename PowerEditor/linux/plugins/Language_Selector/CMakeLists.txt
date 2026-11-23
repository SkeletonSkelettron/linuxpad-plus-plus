# Language_Selector (https://github.com/hassaine-abdellah/npp-language-selector).
# Source revision 405c231bea92b0d141e2fde1843265d3d0875963.
npp_add_plugin(Language_Selector DLLMAIN
	src/PluginDefinition.cpp
	RC src/Language_Selector.rc src/resource.rc)
target_include_directories(Language_Selector PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
