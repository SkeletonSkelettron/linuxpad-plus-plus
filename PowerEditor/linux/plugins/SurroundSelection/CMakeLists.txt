# SurroundSelection (https://github.com/dail8859/SurroundSelection), GPL v2 or later.
# Source revision 916570e9d4bcbc9d7b2046508c6ce685744b220a.
npp_add_plugin(SurroundSelection DLLMAIN
	Main.cpp Dialogs/AboutDialog.cpp Dialogs/Hyperlinks.cpp
	RC Dialogs/resource.rc Dialogs/Version.rc)
target_include_directories(SurroundSelection PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/Dialogs")
