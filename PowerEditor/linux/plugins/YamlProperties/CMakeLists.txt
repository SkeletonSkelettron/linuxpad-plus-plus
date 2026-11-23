# Upstream oiltea94/npp-yaml-properties at 1ce904eda8909931fccc610845b48d23d4dea159.
# The converter uses the same bundled yaml-cpp 0.8.0 as upstream.
file(STRINGS VERSION yaml_properties_version LIMIT_COUNT 1)
file(GLOB yaml_cpp_sources CONFIGURE_DEPENDS "vendor/yaml-cpp/src/*.cpp")
npp_add_plugin(YamlProperties src/plugin.cpp src/converter.cpp ${yaml_cpp_sources})
target_include_directories(YamlProperties PRIVATE "vendor/yaml-cpp/include")
target_compile_definitions(YamlProperties PRIVATE
	YAML_CPP_STATIC_DEFINE YAML_CPP_NO_CONTRIB NPP_YAML_PROPERTIES_VERSION="${yaml_properties_version}")
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/YamlProperties")
install(FILES vendor/yaml-cpp/LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/YamlProperties/yaml-cpp")
