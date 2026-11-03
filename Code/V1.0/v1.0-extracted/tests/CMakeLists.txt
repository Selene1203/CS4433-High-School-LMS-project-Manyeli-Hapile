add_executable(lms_tests
  version_test.cpp types_test.cpp penalty_test.cpp password_user_test.cpp
  mark_test.cpp accounts_test.cpp submission_test.cpp
  marking_moderation_test.cpp security_audit_test.cpp console_ui_test.cpp
  cr1_class_extension_test.cpp)
target_link_libraries(lms_tests PRIVATE lms_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(lms_tests)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_test(NAME system_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/system/system_tests.py $<TARGET_FILE:lms>)
endif()
