# OCaml Lexer (https://github.com/Zaneham/ocaml-npp), Apache License 2.0.
# Source revision aaedbd468c710931352c6ca78badee5a6141e8f8.
# A lexer plugin: the host loads its CreateLexer/GetLexerCount/GetLexerName and OCamlLexer.xml.
npp_add_plugin(OCamlLexer EXPORT_ALL # exports.def on Windows
	src/ocaml_ilexer.cpp
	RC src/OCamlLexer.rc)
target_include_directories(OCamlLexer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_BINARY_DIR}")
# ocaml_config_xml.h (build.sh upstream): the configuration in the plugin, which writes it to the
# plugins configuration folder when it is missing
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/config_xml_head.inc" "${CMAKE_CURRENT_SOURCE_DIR}/config/OCamlLexer.xml"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/config_xml_tail.inc")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/config_xml_head.inc" _head)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/config/OCamlLexer.xml" _xml)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/config_xml_tail.inc" _tail)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ocaml_config_xml.h" "${_head}${_xml}${_tail}")
# the configuration next to the plugin too (the host looks there when the user has none)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config/OCamlLexer.xml" "${CMAKE_BINARY_DIR}/lib/linuxpad++/plugins/OCamlLexer/OCamlLexer.xml" COPYONLY)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/config/OCamlLexer.xml" DESTINATION "lib/linuxpad++/plugins/OCamlLexer")
