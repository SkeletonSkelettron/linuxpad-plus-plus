# CommentToggler (https://github.com/ScienceDiscoverer/CommentToggler), GPL v3.
# Source revision bbae39d806b91fa8ef5e63d3210eb76c6ee919ce.
npp_add_plugin(CommentToggler DLLMAIN
	CommentToggler.cpp PluginDefinition.cpp
	RC CommentToggler.rc)
target_include_directories(CommentToggler PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
