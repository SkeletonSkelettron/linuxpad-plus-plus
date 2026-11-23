# ComparePlus (https://github.com/pnedev/comparePlus), GPL v3 (license.txt); boost regex (src/boost_regex, Boost
# Software License), nlohmann json (src/nlohmann, MIT).
# Source revision 6611373c37a13adc096ae7c4bf1aa0e856e123ed. The sources of its CMakeLists.txt, without the bundled
# git2.dll and sqlite3.dll (libs/) and mingw-std-threads: Compare to Git and SVN load the libgit2 and SQLite of the
# distribution at run time.
npp_add_plugin(ComparePlus DLLMAIN
	src/NppAPI/DockingFeature/StaticDialog.cpp src/AboutDlg/URLCtrl.cpp src/AboutDlg/AboutDialog.cpp
	src/SettingsDlg/ColorCombo.cpp src/SettingsDlg/SettingsDialog.cpp src/CompareOptionsDlg/CompareOptionsDialog.cpp
	src/VisualFiltersDlg/VisualFiltersDialog.cpp src/GitCommitDlg/GitCommitDialog.cpp src/NavDlg/NavDialog.cpp
	src/ProgressDlg/ProgressDlg.cpp src/Engine/Engine.cpp src/Tools.cpp src/UserSettings.cpp src/Compare.cpp
	src/LibGit2/LibGit2Helper.cpp src/NppHelpers.cpp src/LibHelpers.cpp src/SQLite/SqliteHelper.cpp src/Strings.cpp
	RC src/Compare.rc)
target_include_directories(ComparePlus PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/NppAPI" "${CMAKE_CURRENT_SOURCE_DIR}/src/NppAPI/DockingFeature"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Icons" "${CMAKE_CURRENT_SOURCE_DIR}/src/Engine" "${CMAKE_CURRENT_SOURCE_DIR}/src/AboutDlg"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/SettingsDlg" "${CMAKE_CURRENT_SOURCE_DIR}/src/CompareOptionsDlg"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/VisualFiltersDlg" "${CMAKE_CURRENT_SOURCE_DIR}/src/GitCommitDlg"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/NavDlg" "${CMAKE_CURRENT_SOURCE_DIR}/src/ProgressDlg" "${CMAKE_CURRENT_SOURCE_DIR}/src/SQLite"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/LibGit2" "${CMAKE_CURRENT_SOURCE_DIR}/src/boost_regex/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/nlohmann")
target_compile_definitions(ComparePlus PRIVATE MULTITHREAD BOOST_REGEX_STANDALONE)
target_link_libraries(ComparePlus PRIVATE ${CMAKE_DL_LIBS})
# the translations (Settings: language), next to the plugin like on Windows
file(GLOB _cp_localization RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/res/localization" "${CMAKE_CURRENT_SOURCE_DIR}/res/localization/*.json")
foreach(_file IN LISTS _cp_localization)
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/res/localization/${_file}" "${CMAKE_BINARY_DIR}/lib/linuxpad++/plugins/ComparePlus/localization/${_file}" COPYONLY)
	install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/res/localization/${_file}" DESTINATION "lib/linuxpad++/plugins/ComparePlus/localization")
endforeach()
install(FILES license.txt DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/linuxpad++/plugins/ComparePlus")
