include(GoogleTest)
add_library(xgm_header_checks OBJECT integration/include_allocator.cpp)
target_link_libraries(xgm_header_checks PRIVATE xgm::allocator)
target_compile_features(xgm_header_checks PRIVATE cxx_std_17)
xgm_warnings(xgm_header_checks)
add_executable(xgm_tests unit/test_memory.cpp unit/test_arena.cpp unit/test_boundaries.cpp unit/test_alignment.cpp unit/baseline.c)
set_target_properties(xgm_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_link_libraries(xgm_tests PRIVATE xgm::allocator xgm::pool xgm::size_class xgm::arena xgm::tracking xgm::libc_allocator GTest::gtest_main)
target_compile_definitions(xgm_tests PRIVATE XGM_TEST_LIBC)
xgm_warnings(xgm_tests)
gtest_discover_tests(xgm_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS unit TIMEOUT 30)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
add_test(NAME memory_package_consumption COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/integration/check_package.py"
    --source "${PROJECT_SOURCE_DIR}" --work "${PROJECT_BINARY_DIR}/integration"
    --compiler "${CMAKE_C_COMPILER}" --generator "${CMAKE_GENERATOR}"
    --status-source "${XGM_STATUS_SOURCE_DIR}" --prefix "${CMAKE_PREFIX_PATH}")
set_tests_properties(memory_package_consumption PROPERTIES LABELS integration TIMEOUT 180)
