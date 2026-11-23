# NppTaskList (https://github.com/Megabyteceer/npp-task-list).
# Source revision 7d53ea90d34b099cbd45d574ad3f3dcfa2938874.
npp_add_plugin(NppTaskList DLLMAIN
	src/Common.cpp src/NppDarkModeDummy.cpp src/NppTaskListPlugin.cpp src/PluginDefinition.cpp src/StaticDialog.cpp src/TaskListDlg.cpp src/URLCtrl.cpp src/config.cpp src/dpiManagerV2.cpp src/AboutDialog/AboutDlg.cpp
	RC resources/TaskList.rc)
target_include_directories(NppTaskList PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/AboutDialog")
install(FILES COPYING DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/NppTaskList")
