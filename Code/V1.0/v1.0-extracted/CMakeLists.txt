cmake_minimum_required(VERSION 3.16)
project(ModeratedLMS VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LMS_BUILD_TESTS "Build the unit and integration tests" ON)
option(LMS_ENABLE_COVERAGE "Instrument the build for gcov/lcov coverage" OFF)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

# The schema comes straight from the design document so there is one copy only.
file(READ ${CMAKE_SOURCE_DIR}/docs/phase3/db/schema.sql LMS_SCHEMA_SQL)
configure_file(src/schema_sql.cpp.in ${CMAKE_BINARY_DIR}/schema_sql.cpp @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/docs/phase3/db/schema.sql)

# Strict warnings for our own targets only (not third-party code).
add_library(lms_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(lms_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wold-style-cast)
endif()

# Core library: all business logic + data access. The executable and the
# tests both link against it, so everything is testable without the UI.
add_library(lms_core
  src/version.cpp src/types.cpp src/password.cpp src/user.cpp
  src/penalty_strategy.cpp src/domain.cpp src/database.cpp
  src/sqlite_repository.cpp src/services_accounts.cpp src/services_work.cpp
  src/console_ui.cpp src/demo_data.cpp ${CMAKE_BINARY_DIR}/schema_sql.cpp)
target_include_directories(lms_core PUBLIC include)
target_link_libraries(lms_core PUBLIC SQLite::SQLite3 OpenSSL::Crypto PRIVATE lms_warnings)

if(LMS_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(lms_core PRIVATE --coverage -O0 -g)
  target_link_libraries(lms_core PUBLIC --coverage)
endif()

add_executable(lms src/main.cpp)
target_link_libraries(lms PRIVATE lms_core lms_warnings)

if(LMS_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  enable_testing()
  add_subdirectory(tests)
endif()
