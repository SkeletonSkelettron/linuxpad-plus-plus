# urlPlugin (https://github.com/SinghRajenM/nppURLPlugin), GPL v2 or later.
# Source revision 6b835e4fa7f0ff1f34d227fe07f94b3ef8dd3b70.
npp_add_plugin(urlPlugin DLLMAIN
	src/urlPlugin/AboutDlg.cpp src/urlPlugin/PluginHelper.cpp src/urlPlugin/Profile.cpp src/urlPlugin/ScintillaEditor.cpp src/urlPlugin/SettingsDlg.cpp src/urlPlugin/ShortcutCommand.cpp src/urlPlugin/dllmain.cpp src/urlPlugin/external/npp/ControlsTab.cpp src/urlPlugin/external/npp/StaticDialog.cpp src/urlPlugin/external/npp/TabBar.cpp src/urlPlugin/external/npp/URLCtrl.cpp src/utilityLib/Execute.cpp src/utilityLib/StringHelper.cpp src/utilityLib/Utility.cpp src/utilityLib/pch.cpp
	RC src/urlPlugin/urlPlugin.rc)
target_include_directories(urlPlugin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src/urlPlugin" "${CMAKE_CURRENT_SOURCE_DIR}/src/urlPlugin/external/npp" "${CMAKE_CURRENT_SOURCE_DIR}/src/utilityLib")
