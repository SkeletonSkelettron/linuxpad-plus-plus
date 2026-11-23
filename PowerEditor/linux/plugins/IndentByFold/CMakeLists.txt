# IndentByFold (https://github.com/ffes/indentbyfold), GPL v2 or later.
# Source revision 2ebc630a63d47144b878d948cd1f89f5ba7d9126.
npp_add_plugin(IndentByFold DLLMAIN
	IBFMenu.cpp IBFPlugin.cpp IndentByFold.cpp NppMessager.cpp NppPlugin.cpp NppPluginMenu.cpp SciMessager.cpp WaitCursor.cpp
	RC IndentByFold_res.rc)
