# NppExec (https://github.com/d0vgan/nppexec), GPL v3 (LICENSE).
# Source revision 7a5d279468b7929146758b0d24a78e0d76d17020. The sources of NppExec_VC2019.vcxproj.
npp_add_plugin(NppExec DLLMAIN
	src/CAnyCheckBox.cpp src/CAnyComboBox.cpp src/CAnyListBox.cpp src/CAnyRichEdit.cpp src/CAnyWindow.cpp src/CFileModificationChecker.cpp 
	src/ChildProcess.cpp src/CPopupListBox.cpp src/cpp/CFileBufT.cpp src/CSimpleLogger.cpp src/CStaticOptionsManager.cpp src/c_base/HexStr.c 
	src/c_base/int2str.c src/c_base/MatchMask.c src/c_base/max_int.c src/c_base/PackDataStr.c src/c_base/str2int.c src/c_base/str_func.c 
	src/DirFileLister.cpp src/DlgAdvancedOptions.cpp src/DlgConsole.cpp src/DlgConsoleEncoding.cpp src/DlgConsoleOutputFilter.cpp src/DlgDoExec.cpp 
	src/DlgHelpAbout.cpp src/DlgInputBox.cpp src/encodings/SysUniConv.cpp src/fparser/fparser.cc src/PickColorBtn.cpp src/NppExec.cpp 
	src/NppExecCommandExecutor.cpp src/NppExecScriptEngine.cpp src/NppExecHelpers.cpp src/NppScript.cpp src/NppScriptList.cpp 
	src/PluginCommunication/NppExecPluginInterface.cpp src/WarningAnalyzer.cpp src/WarningMaskMatcher.cpp 
	RC src/NppExec.rc)
target_include_directories(NppExec PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# c_base: C files that include <windows.h> (C++ in the SDK)
file(GLOB _nppexec_c_base "${CMAKE_CURRENT_SOURCE_DIR}/src/c_base/*.c")
set_source_files_properties(${_nppexec_c_base} PROPERTIES LANGUAGE CXX)
