# EmojiDescription (https://github.com/Ruberoid/npp_emoji_description).
# Source revision 96229c12bbd60f754fc5acae18dd788333770467.
npp_add_plugin(EmojiDescription DLLMAIN
	src/CharDetailsDialog.cpp src/EmojiDescription.cpp src/PluginDefinition.cpp src/UnicodeNames.cpp src/DockingFeature/GoToLineDlg.cpp src/DockingFeature/StaticDialog.cpp
	RC src/EmojiDescription.rc src/DockingFeature/goLine.rc RC_INCLUDES "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(EmojiDescription PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/DockingFeature")
# the names of the characters (src/UnicodeNames.bin upstream): made from UnicodeData.txt at configure time
set(_unicode_names "${CMAKE_CURRENT_SOURCE_DIR}/tools/unicode_names")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_unicode_names}/gen_names.py" "${_unicode_names}/UnicodeData.txt")
execute_process(COMMAND "${Python3_EXECUTABLE}" -I "${_unicode_names}/gen_names.py" "${_unicode_names}/UnicodeData.txt"
		"${CMAKE_CURRENT_BINARY_DIR}/UnicodeNames.bin"
	RESULT_VARIABLE _unicode_names_result OUTPUT_QUIET)
if(NOT _unicode_names_result EQUAL 0)
	message(FATAL_ERROR "EmojiDescription: gen_names.py failed")
endif()
