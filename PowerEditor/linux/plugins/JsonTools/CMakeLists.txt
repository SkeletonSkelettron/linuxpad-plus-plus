option(NPP_BUILD_JSONTOOLS "Build the JSONTools plugin when the .NET 10 SDK is available" ON)
if(NOT NPP_BUILD_JSONTOOLS)
	return()
endif()

find_program(NPP_DOTNET_EXECUTABLE dotnet)
if(NPP_DOTNET_EXECUTABLE)
	execute_process(COMMAND "${NPP_DOTNET_EXECUTABLE}" --list-sdks OUTPUT_VARIABLE _sdk_versions)
endif()
if(NOT NPP_DOTNET_EXECUTABLE OR NOT _sdk_versions MATCHES "(^|[\r\n])10\\.")
	message(STATUS "JSONTools skipped: install the .NET 10 SDK to build its portable engine")
	return()
endif()

npp_add_plugin(JsonTools DLLMAIN JsonTools.cpp)

# Portable sources from molsonkiko/JsonToolsNppPlugin, revision aca51ea651948a9429317f1cf130693e3c8ae542.
set(_backend "${CMAKE_CURRENT_SOURCE_DIR}/backend")
set(_output "${CMAKE_BINARY_DIR}/lib/notepad++/plugins/JsonTools")
file(GLOB_RECURSE _sources CONFIGURE_DEPENDS "${_backend}/*.cs")
set(_managed_files
	"${_output}/JsonToolsEngine.dll"
	"${_output}/JsonToolsEngine.deps.json"
	"${_output}/JsonToolsEngine.runtimeconfig.json")
add_custom_command(
	OUTPUT ${_managed_files}
	COMMAND "${CMAKE_COMMAND}" -E env
		"DOTNET_CLI_HOME=${CMAKE_CURRENT_BINARY_DIR}/dotnet-home"
		DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1
		"${NPP_DOTNET_EXECUTABLE}" build "${_backend}/JsonToolsEngine.csproj"
		--configuration Release --artifacts-path "${CMAKE_CURRENT_BINARY_DIR}/dotnet"
		--output "${_output}" "-p:RestoreConfigFile=${_backend}/NuGet.Config" -p:UseSharedCompilation=false
	DEPENDS ${_sources} "${_backend}/JsonToolsEngine.csproj" "${_backend}/NuGet.Config"
	COMMENT "Building the portable JSONTools engine"
	VERBATIM)
add_custom_target(JsonTools_backend DEPENDS ${_managed_files})
add_dependencies(JsonTools JsonTools_backend)
install(FILES ${_managed_files} DESTINATION "lib/notepad++/plugins/JsonTools")
