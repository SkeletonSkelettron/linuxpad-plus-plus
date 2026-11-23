# pork2sausage (https://github.com/npp-plugins/pork2sausage), GPL v2 or later.
# Source revision cce79843879d66469f8c5d6459e2f49f11e6fa53.
npp_add_plugin(pork2sausage DLLMAIN
	src/Pork2Sausage.cpp src/Process.cpp
	RC src/Pork2Sausage.rc)
target_include_directories(pork2sausage PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(pork2sausage PRIVATE Qt6::Core)
# the default commands (Linux ones) next to the plugin: copied to the plugins configuration folder at the first start
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Config/pork2sausage.ini" "${CMAKE_BINARY_DIR}/lib/linuxpad++/plugins/pork2sausage/Config/pork2sausage.ini" COPYONLY)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/Config/pork2sausage.ini" DESTINATION "lib/linuxpad++/plugins/pork2sausage/Config")
