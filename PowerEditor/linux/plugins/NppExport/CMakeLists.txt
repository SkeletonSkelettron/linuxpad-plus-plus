# NppExport (https://github.com/chcg/NPP_ExportPlugin).
# Source revision 750cda485a16d48c3dec5b5b0b8a65f252a0b9d0.
npp_add_plugin(NppExport DLLMAIN
	src/Exporter.cpp src/HTMLExporter.cpp src/NppExport.cpp src/RTFExporter.cpp src/TXTExporter.cpp src/stdafx.cpp
	RC src/resource.rc)
target_include_directories(NppExport PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
