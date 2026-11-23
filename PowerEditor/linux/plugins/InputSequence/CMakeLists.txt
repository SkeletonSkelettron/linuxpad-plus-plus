# InputSequence (https://github.com/dasu88/NPPInputSequence).
# Source revision 03e8b0c7fff41d42b69913c2fa08a04ecf6224d2.
npp_add_plugin(InputSequence DLLMAIN
	src/InputSequencePlugin.cpp)
target_include_directories(InputSequence PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
