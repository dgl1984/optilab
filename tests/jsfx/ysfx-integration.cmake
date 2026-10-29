# ysfx is the top-level project so its dependency paths remain valid.
get_filename_component(OPTILAB_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(OPTILAB_BUILD_JUCE_PLUGINS OFF CACHE BOOL "" FORCE)
set(OPTILAB_BUILD_WINAMP OFF CACHE BOOL "" FORCE)
set(OPTILAB_BUILD_FOOBAR2000 OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory("${OPTILAB_ROOT}/native" optilab-native)
add_executable(optilab-jsfx-regression "${OPTILAB_ROOT}/tests/JsfxRegression.cpp")
set_target_properties(optilab-jsfx-regression PROPERTIES CXX_STANDARD 20)
target_link_libraries(optilab-jsfx-regression PRIVATE ysfx::ysfx optilab-core)

add_executable(optilab-jsfx-legacy-state-regression "${OPTILAB_ROOT}/tests/JsfxLegacyStateTest.cpp")
set_target_properties(optilab-jsfx-legacy-state-regression PROPERTIES CXX_STANDARD 20)
target_link_libraries(optilab-jsfx-legacy-state-regression PRIVATE ysfx::ysfx optilab-core)
