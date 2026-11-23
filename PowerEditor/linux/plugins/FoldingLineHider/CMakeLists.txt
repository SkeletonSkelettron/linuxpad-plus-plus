# FoldingLineHider (https://github.com/leonardchai/FoldingLineHider), GPL v2 or later.
# Source revision 0c43f177027ba7747d5b30fd1322eb469273e90c.
npp_add_plugin(FoldingLineHider DLLMAIN
	src/NppPlugin.cpp src/PluginDefinition.cpp
	RC src/NppPlugin.rc)
target_include_directories(FoldingLineHider PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
