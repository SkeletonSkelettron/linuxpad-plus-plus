# SqlColumnHighlighter (https://github.com/zhgao/npp-sql-column-highlighter), MIT.
# Source revision 19707da2780376b7876e54859ac4eee4c002436f.
npp_add_plugin(SqlColumnHighlighter
	src/Highlighter.cpp src/PluginMain.cpp
	RC src/version.rc)
target_include_directories(SqlColumnHighlighter PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
