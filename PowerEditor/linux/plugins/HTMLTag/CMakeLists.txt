# HTML Tag (https://github.com/rdipardo/nppHTMLTag), MPL 2.0.
# Source revision 4bac411c58a4f37706681756b3d4623b44bf15a0; external/simpleini: brofield/simpleini v4.22 (MIT),
# src/DockingFeature: npp-plugins/plugintemplate abd8c23 (fetched with CPM upstream).
npp_add_plugin(HTMLTag DLLMAIN
	src/Entities.cpp src/HtmlTag.cpp src/TagFinder.cpp src/Unicode.cpp src/Forms/AboutDlg.cpp
	src/LibNppPlugin/FuncArray.cpp src/LibNppPlugin/LocalizedPlugin.cpp src/LibNppPlugin/PluginBase.cpp
	src/LibNppPlugin/include/SciTextObjects.cpp src/LibNppPlugin/include/TextConv.cpp src/prj/DllMain.cpp
	src/DockingFeature/StaticDialog.cpp external/simpleini/ConvertUTF.c
	RC src/prj/HTMLTag.rc RC_INCLUDES src/Forms)
target_include_directories(HTMLTag PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/Forms"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/LibNppPlugin" "${CMAKE_CURRENT_SOURCE_DIR}/src/LibNppPlugin/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/DockingFeature" "${CMAKE_CURRENT_SOURCE_DIR}/external/simpleini")
# entities and translations next to the plugin
foreach(_file HTMLTag-entities.ini HTMLTag-localizations.ini HTMLTag-dialogs.ini)
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/dat/${_file}" "${CMAKE_BINARY_DIR}/lib/linuxpad++/plugins/HTMLTag/${_file}" COPYONLY)
	install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/dat/${_file}" DESTINATION "lib/linuxpad++/plugins/HTMLTag")
endforeach()
