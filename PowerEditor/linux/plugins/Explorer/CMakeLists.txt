# Explorer (https://github.com/oviradoi/npp-explorer-plugin), GPL v2 (Explorer/license.txt).
# Source revision ee2cfce890456af04e30068eb7a757c2487e3fe2. The sources of Explorer/projects/VC2015/Explorer.vcxproj.
npp_add_plugin(Explorer DLLMAIN
	Common/Ccpp/ImageListSet.cpp Common/Ccpp/StaticDialog.cpp Common/Ccpp/SysMsg.cpp Common/Ccpp/ToolBar.cpp Explorer/src/Explorer.cpp 
	Explorer/src/ExplorerDialog.cpp Explorer/src/FavesDialog.cpp Explorer/src/FileDlg/FileDlg.cpp Explorer/src/FileList/ComboOrgi.cpp 
	Explorer/src/FileList/FileList.cpp Explorer/src/HelpDlg/HelpDialog.cpp Explorer/src/MISC/ContextMenu.cpp Explorer/src/MISC/TreeHelperClass.cpp 
	Explorer/src/NewDlg/NewDlg.cpp Explorer/src/OptionDlg/OptionDialog.cpp Explorer/src/PropDlg/PropDlg.cpp Explorer/src/PureDnD/DragDropImpl.cpp 
	Explorer/src/stdafx.cpp 
	RC Explorer/src/ExplorerDialog.rc)
target_include_directories(Explorer PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/Common/Ccpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/src"
	"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/src/NewDlg"
	"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/src/PropDlg"
	"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/src/Toolbar"
	"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/src/FileDlg"
	"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/src/FileList"
	"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/src/MISC"
	"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/src/HelpDlg"
	"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/src/OptionDlg"
	"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/src/PureDnD"
)
install(FILES Explorer/license.txt DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/Explorer")
