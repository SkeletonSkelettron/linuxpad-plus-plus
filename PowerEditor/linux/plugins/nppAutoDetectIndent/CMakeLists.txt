# Auto Detect Indention (https://github.com/Chocobo1/nppAutoDetectIndent), GPL v3 or later.
# Source revision 1c8af156d330d169c406e5fa7aae133830961aff.
npp_add_plugin(nppAutoDetectIndent DLLMAIN
	dllmain.cpp nppAutoDetectIndent.cpp settings.cpp
	RC nppAutoDetectIndent.rc)
target_compile_definitions(nppAutoDetectIndent PRIVATE NPPAUTODETECTINDENT_EXPORTS)
