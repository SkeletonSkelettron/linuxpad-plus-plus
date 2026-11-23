# XML Tools (https://github.com/morbac/xmltools), GPL v3 (LICENSE).
# Source revision 771a82c1fc7c3684e112d5b327d1d89b870c1078. The sources of XMLTools.vcxproj and of the QuickXml,
# SimpleXml and StringXml libraries; on Linux, libxml2 (linux/LibXml2Wrapper.cpp, libxslt loaded at run time) in the
# place of MSXML and Qt dialogs (linux/XmlToolsDialogs.cpp) in the place of the MFC ones.
find_package(LibXml2 QUIET)
if(NOT LibXml2_FOUND)
	message(STATUS "XMLTools: libxml2 (libxml2-devel) not found, the plugin is not built")
	return()
endif()
npp_add_plugin(XMLTools
	Debug.cpp Config.cpp Report.cpp nppHelpers.cpp nppMenu.cpp
	ToolsComment.cpp ToolsConvert.cpp ToolsEditor.cpp ToolsErrors.cpp ToolsPrettyPrintFast.cpp ToolsXLSTransform.cpp
	ToolsXPath.cpp ToolsXmlValidate.cpp XMLTools.cpp
	QuickXmlLib/QuickXml/src/XmlFormater.cpp QuickXmlLib/QuickXml/src/XmlParser.cpp
	SimpleXmlLib/SimpleXml/src/ChunkedStream.cpp SimpleXmlLib/SimpleXml/src/Lexer.cpp SimpleXmlLib/SimpleXml/src/PrettyPrinter.cpp
	SimpleXmlLib/SimpleXml/src/SimpleXml.cpp SimpleXmlLib/SimpleXml/src/TextEncoding.cpp
	StringXmlLib/StringXml/src/StringXml.cpp
	linux/LibXml2Wrapper.cpp linux/XmlToolsDialogs.cpp
	RC XMLTools.rc)
target_include_directories(XMLTools PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/QuickXmlLib/QuickXml/src"
	"${CMAKE_CURRENT_SOURCE_DIR}/SimpleXmlLib/SimpleXml/src"
	"${CMAKE_CURRENT_SOURCE_DIR}/StringXmlLib/StringXml/src")
target_link_libraries(XMLTools PRIVATE LibXml2::LibXml2 ${CMAKE_DL_LIBS})
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/XMLTools")
