# RAScript (https://github.com/joshraphael/npp-rascript), MIT.
# Source revision 646d5c3e32319689d3492ca01476260daf166281 (1.1.0); src/RAScript.rc from templates/RAScript.rc.template.
# src/RAScript.xml: joshraphael/rascript-syntax v0.5.1 (RASCRIPT_SYNTAX_VERSION of its Makefile).
# external/tinyxml2: leethomason/tinyxml2 11.0.0 (zlib). Use the editor's bundled Boost.Regex.
npp_add_plugin(RAScript DLLMAIN EXPORT_ALL LEXLIB # Exports.def on Windows
	src/LexRAScript.cpp src/Parser.cpp src/RAScript.cpp external/tinyxml2/tinyxml2.cpp
	RC src/RAScript.rc)
target_include_directories(RAScript PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/external/tinyxml2" "${BOOST_DIR}")
target_compile_definitions(RAScript PRIVATE BOOST_REGEX_STANDALONE)
# the configuration next to the plugin too (the plugin writes it to the plugins configuration folder)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/RAScript.xml" "${CMAKE_BINARY_DIR}/lib/linuxpad++/plugins/RAScript/RAScript.xml" COPYONLY)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/RAScript.xml" DESTINATION "lib/linuxpad++/plugins/RAScript")
