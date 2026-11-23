# HiddenLexers (https://github.com/pryrt/NppPlugin-HiddenLexers), GPL v3 (license.txt).
# Source revision df9c13cfbb43124db89a72db19529f2730a4ceb0.
npp_add_plugin(HiddenLexers DLLMAIN
	src/HiddenLexers.cpp src/PluginDefinition.cpp src/Classes/ConfigFromJson.cpp src/Classes/HiddenLexerInterface.cpp src/Classes/NppMetaClass.cpp src/Dialogs/AboutDialog.cpp src/lib/Hyperlinks.cpp src/lib/pcjHelper.cpp
	RC src/HiddenLexers.rc src/Dialogs/AboutDialog.rc)
target_include_directories(HiddenLexers PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/Classes" "${CMAKE_CURRENT_SOURCE_DIR}/src/Dialogs" "${CMAKE_CURRENT_SOURCE_DIR}/src/lib")
