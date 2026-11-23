# JSTool (https://github.com/sunjw/jstoolnpp), GPL v2 or later (LICENSE).
# Source revision 277fdbe62439ab11842c6e5595c365d96ef9f87b. The sources of JSToolNpp/JSMinNPP.vcxproj.
npp_add_plugin(JSMinNPP DLLMAIN
	src/aboutDlg.cpp src/EditControlEx.cpp src/IniFileProcessor.cpp src/IniProcessor.cpp src/IniValue.cpp src/jsformatString.cpp src/jsmin.cpp 
	src/jsminCharArray.cpp src/jsMinNpp.cpp src/JSONDialog.cpp src/jsonparser.cpp src/jsonStringProc.cpp src/JsonTree.cpp src/jsonValue.cpp 
	src/jsparser.cpp src/optionsDlg.cpp src/realjsformatter.cpp src/StaticDialog.cpp src/strhelper.cpp src/TreeViewController.cpp src/utility.cpp 
	RC src/jsMinNpp.rc)
target_include_directories(JSMinNPP PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/DockingFeature")
