# BetterMultiSelection (https://github.com/dail8859/BetterMultiSelection), GPL v2 or later.
# Source revision 4c63c6dd8eaf4f2df5a56c87ab79a57bb9c671c4.
npp_add_plugin(BetterMultiSelection DLLMAIN
	Main.cpp UniConversion.cpp Dialogs/AboutDialog.cpp Dialogs/Hyperlinks.cpp
	RC Dialogs/resource.rc Dialogs/Version.rc)
target_include_directories(BetterMultiSelection PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/Dialogs")
