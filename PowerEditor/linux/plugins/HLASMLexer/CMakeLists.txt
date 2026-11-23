# HLASM Lexer (https://github.com/Zaneham/hlasm-npp), Apache License 2.0.
# Source revision fe7541565cd0e0af319b08600a80629dd46fb45e.
# A lexer plugin: the host loads its CreateLexer/GetLexerCount/GetLexerName and HLASMLexer.xml.
npp_add_plugin(HLASMLexer EXPORT_ALL # exports.def on Windows
	src/hlasm_ilexer.cpp
	RC src/HLASMLexer.rc)
target_include_directories(HLASMLexer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_BINARY_DIR}")
# hlasm_config_xml.h (build.sh upstream): the configuration in the plugin, which writes it to the
# plugins configuration folder when it is missing
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/config_xml_head.inc" "${CMAKE_CURRENT_SOURCE_DIR}/config/HLASMLexer.xml"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/config_xml_tail.inc")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/config_xml_head.inc" _head)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/config/HLASMLexer.xml" _xml)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/config_xml_tail.inc" _tail)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/hlasm_config_xml.h" "${_head}${_xml}${_tail}")
# the configuration next to the plugin too (the host looks there when the user has none)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config/HLASMLexer.xml" "${CMAKE_BINARY_DIR}/lib/linuxpad++/plugins/HLASMLexer/HLASMLexer.xml" COPYONLY)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/config/HLASMLexer.xml" DESTINATION "lib/linuxpad++/plugins/HLASMLexer")
