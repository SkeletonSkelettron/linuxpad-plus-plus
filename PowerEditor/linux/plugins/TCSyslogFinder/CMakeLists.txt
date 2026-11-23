# TCSyslogFinder (https://github.com/bsagarzazu/npp-tc-syslog-finder).
# Source revision db543160aa4b33572d373d1e588d79603c7bd1c0.
npp_add_plugin(TCSyslogFinder DLLMAIN
	src/PluginDefinition.cpp src/TCSyslogFinder.cpp
	RC src/TCSyslogFinder.rc)
target_include_directories(TCSyslogFinder PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
