# NPPColorPicker (https://github.com/dasu88/NPPColorPicker).
# Source revision 01f052696b77781e44dd01277727f5262ffd43a1.
npp_add_plugin(NPPColorPicker
	src/ColorPicker.cpp
	RC src/ColorPicker.rc)
target_include_directories(NPPColorPicker PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
