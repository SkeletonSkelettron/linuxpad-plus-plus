# HexEditor (https://github.com/chcg/NPP_HexEditor), GPL v2 (HexEditor/license.txt).
# Source revision 026debb7d650f87a27f1199d945c4b3518064662. The sources of HexEditor/projects/2003/HexEditor.vcxproj.
npp_add_plugin(HexEditor DLLMAIN
	Common/Ccpp/Common.cpp Common/Ccpp/NppDarkModeDummy.cpp Common/Ccpp/StaticDialog.cpp Common/Ccpp/dpiManagerV2.cpp HexEditor/src/HEXDialog.cpp HexEditor/src/Hex.cpp HexEditor/src/Utf8_16.cpp HexEditor/src/HelpDlg/HelpDialog.cpp HexEditor/src/HelpDlg/URLCtrl.cpp HexEditor/src/OptionDlg/ColumnDialog.cpp HexEditor/src/OptionDlg/OptionDialog.cpp HexEditor/src/UserDlg/CompareDialog.cpp HexEditor/src/UserDlg/FindReplaceDialog.cpp HexEditor/src/UserDlg/GotoDialog.cpp HexEditor/src/UserDlg/PatternDialog.cpp HexEditor/src/misc/ColorCombo.cpp HexEditor/src/misc/ColorPopup.cpp HexEditor/src/misc/ModifyMenu.cpp HexEditor/src/misc/MultiTypeCombo.cpp
	RC HexEditor/src/HEXDialog.rc)
target_include_directories(HexEditor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/Common/Ccpp" "${CMAKE_CURRENT_SOURCE_DIR}/HexEditor/src" "${CMAKE_CURRENT_SOURCE_DIR}/HexEditor/src/HelpDlg" "${CMAKE_CURRENT_SOURCE_DIR}/HexEditor/src/OptionDlg" "${CMAKE_CURRENT_SOURCE_DIR}/HexEditor/src/UserDlg" "${CMAKE_CURRENT_SOURCE_DIR}/HexEditor/src/misc" "${CMAKE_CURRENT_SOURCE_DIR}/NativeLang/src")
# Windows 7 and later: the structures HexEditor declares for older Windows SDKs (COMBOBOXINFO) are left out
target_compile_definitions(HexEditor PRIVATE WINVER=0x0601 _WIN32_WINNT=0x0601)
install(FILES HexEditor/license.txt DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/HexEditor")
