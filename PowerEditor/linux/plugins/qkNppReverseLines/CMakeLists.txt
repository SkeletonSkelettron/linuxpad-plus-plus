# qkNppReverseLines (https://github.com/querykuma/qkNppReverseLines), GPL v3 or later.
# Source revision c15eb32e465f2b1ce8d7b025511140af6789d025.
npp_add_plugin(qkNppReverseLines DLLMAIN
	src/NppReverseLines.cpp src/PluginDefinition.cpp
	RC src/NppReverseLines.rc)
target_include_directories(qkNppReverseLines PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
