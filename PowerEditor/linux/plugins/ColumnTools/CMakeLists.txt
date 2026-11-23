# ColumnTools (https://github.com/vinsworldcom/nppColumnTools), GPL v2 or later.
# Source revision e7c4d6fe2dac40edd560ad1300a398666ef6c904.
npp_add_plugin(ColumnTools DLLMAIN
	ColumnTools.cpp HorizontalRuler.cpp Ini.cpp QtHorizontalRuler.cpp PluginDefinition.cpp SettingsDlg.cpp
	RC ColumnTools.rc)
target_include_directories(ColumnTools PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
install(FILES COPYING DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/ColumnTools")
