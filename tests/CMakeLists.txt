find_package(GTest)

enable_testing()

add_executable(test_requeue test_requeue.cpp)

target_link_libraries(test_requeue PUBLIC GTest::gtest GTest::gtest_main v4l2_utils)

add_test(NAME TestRequeue COMMAND test_requeue)