# selectNLaunch (https://github.com/npp-plugins/selectnlaunch), GPL v2 or later.
# Source revision d124bb18404fdd218c453cbde5f1c8fe3bc8456b.
npp_add_plugin(selectNLaunch DLLMAIN
	src/SelectNLaunch.cpp
	RC src/SelectNLaunch.rc)
target_include_directories(selectNLaunch PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
