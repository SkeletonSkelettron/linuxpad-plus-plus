# JSON Viewer (https://github.com/NPP-JSONViewer/JSON-Viewer), MIT.
# Source revision fcd2ae56453fd2acf0fb8197110f6d6cd700f30f; external/rapidjson: NPP-JSONViewer/rapidjson e0921584 (MIT),
# DockingFeature: npp-plugins/plugintemplate 1b1fa441 (its npp_include points there).
npp_add_plugin(NPPJSONViewer DLLMAIN
	AboutDlg.cpp JsonHandler.cpp JsonViewDlg.cpp NppJsonPlugin.cpp Profile.cpp RapidJsonHandler.cpp
	ScintillaEditor.cpp SettingsDlg.cpp ShortcutCommand.cpp SliderCtrl.cpp TreeViewCtrl.cpp dllmain.cpp
	DockingFeature/StaticDialog.cpp
	UtilityLib/Execute.cpp UtilityLib/StringHelper.cpp UtilityLib/Utility.cpp
	RC resource.rc)
target_include_directories(NPPJSONViewer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/UtilityLib" "${CMAKE_CURRENT_SOURCE_DIR}/external")
