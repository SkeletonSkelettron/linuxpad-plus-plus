# Notepad++ Converter (https://github.com/npp-plugins/converter), GPL v3.
# Source revision dbba091853eb27580ea8418bba1c5a907885d211.
npp_add_plugin(NppConverter DLLMAIN
	NppPluginDemo.cpp PluginDefinition.cpp conversionPanel.cpp DockingFeature/StaticDialog.cpp
	RC conversionPanel.rc)
