# Notepad++ Syntax Plugin for Foresight Platform Fore (https://github.com/rhyots/notepad-plus-plus-syntax-plugin-foresight-platform-fore), MIT.
# Source revision 5c62dbe5d80dd046a3660b9df5bdf72efbb4e084. A lexer plugin (ForeLexer.def on Windows).
npp_add_plugin(ForeLexer DLLMAIN EXPORT_ALL
	src/AutoDetect.cpp src/ForeDetect.cpp src/ForeLexer.cpp src/Plugin.cpp src/ThemePalette.cpp
	RC src/ForeLexer.rc)
target_include_directories(ForeLexer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# the light configuration next to the plugin too (the plugin writes it to the plugins configuration folder)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/styles/ForeLexer-Light.xml" "${CMAKE_BINARY_DIR}/lib/linuxpad++/plugins/ForeLexer/ForeLexer.xml" COPYONLY)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/styles/ForeLexer-Light.xml" DESTINATION "lib/linuxpad++/plugins/ForeLexer" RENAME ForeLexer.xml)
