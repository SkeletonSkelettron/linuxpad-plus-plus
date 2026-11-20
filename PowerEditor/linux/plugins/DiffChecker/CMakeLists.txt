find_package(Qt6 6.4 REQUIRED COMPONENTS Concurrent)

# engine/: portable ComparePlus algorithms, revision 6611373c37a13adc096ae7c4bf1aa0e856e123ed.
npp_add_plugin(DiffChecker DiffChecker.cpp DiffModel.cpp)
target_link_libraries(DiffChecker PRIVATE Qt6::Concurrent)
