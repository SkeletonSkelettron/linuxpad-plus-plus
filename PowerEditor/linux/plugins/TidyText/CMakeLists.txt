# TidyText (https://github.com/gonzalu/TidyText), GPL v2 or later.
# Source revision ab3809e132a4ab0b479fb8a42d509b1a16b75889.
npp_add_plugin(TidyText DLLMAIN
	src/ContextMenu.cpp src/NonPrintingChars.cpp src/PluginDefinition.cpp src/SendToAI.cpp src/TidyText.cpp
	RC src/TidyText.rc)
target_include_directories(TidyText PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
