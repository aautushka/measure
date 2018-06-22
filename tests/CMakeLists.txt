project(tests CXX)

enable_testing()
#find_package(GTest REQUIRED)
include_directories (..)

set(SRC 
  metric_monitor_tests
  metric_trie_tests
)

add_executable(tests ${SRC})

target_link_libraries(tests gtest gtest_main pthread)
