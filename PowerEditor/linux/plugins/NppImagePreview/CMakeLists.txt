# Qt port of bdgeek/NppImagePreview at 4bc01beee354fba6f1d0729de905ced4864e13e0.
npp_add_plugin(NppImagePreview ImagePreview.cpp)
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/NppImagePreview")
