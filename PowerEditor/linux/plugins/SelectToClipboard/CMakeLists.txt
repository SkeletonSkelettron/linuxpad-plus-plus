# SelectToClipboard (https://github.com/KubaDee/SelectToClipboard), GPL v2 or later.
# Source revision d38ddc293a57d8b0c27e953c892243c6e6980ac0.
npp_add_plugin(SelectToClipboard DLLMAIN
	src/PluginDefinition.cpp src/SelectToClipboard.cpp
	RC src/SelectToClipboard.rc)
target_include_directories(SelectToClipboard PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
