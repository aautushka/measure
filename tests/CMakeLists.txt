project(tests CXX)

enable_testing()
find_package(GTest REQUIRED)
include_directories (..)

add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
add_link_options(-fsanitize=address)

set(SRC 
  metric_monitor_tests.cpp
  metric_trie_tests.cpp
)

add_executable(tests ${SRC})

target_link_libraries(tests GTest::gtest GTest::gtest_main pthread)
target_compile_features(tests PRIVATE cxx_std_17)
