# NppLdapFormatter (https://github.com/ignaciojulio/npp-ldap-formatter), MIT.
# Source revision 21c66c880bdee037c61c01e756aa17127f39b163.
npp_add_plugin(NppLdapFormatter DLLMAIN
	src/BuildInfo.cpp src/CoreBudget.cpp src/LdapParser.cpp src/PluginController.cpp src/PluginDefinition.cpp src/PluginDiagnostics.cpp src/PluginExports.cpp src/PluginWorker.cpp src/ScintillaGateway.cpp)
target_include_directories(NppLdapFormatter PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/NppLdapFormatter")
