cmake_minimum_required(VERSION 3.20)
project(safar VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SAFAR_BUILD_TESTS "Build unit tests" ON)
option(SAFAR_WERROR "Treat warnings as errors" OFF)
option(SAFAR_SANITIZE "Build with ASan + UBSan" OFF)

# ---- Dependencies (downloaded once by CMake, nothing to install by hand) ----
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

FetchContent_Declare(httplib
  URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.18.1.tar.gz)
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)

FetchContent_Declare(spdlog
  URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.3.tar.gz)

FetchContent_MakeAvailable(httplib json spdlog)

# ---- Core library: everything except main(), so tests can link it ----
add_library(safar_core
  src/api/server.cpp
  src/api/lab_routes.cpp
  src/api/journey_routes.cpp
  src/api/fare_routes.cpp
  src/api/params.cpp
  src/api/disruption_routes.cpp
  src/api/trip_routes.cpp
  src/disruption/closures.cpp
  src/app/context.cpp
  src/data/dataset.cpp
  src/fares/transport_config.cpp
  src/fares/adapters.cpp
  src/fares/fare_engine.cpp
  src/graph/graph.cpp
  src/journey/planner.cpp
  src/optimize/tour.cpp
  src/roads/road_matrix.cpp
  src/search/search.cpp
  src/search/uninformed.cpp
  src/search/best_first.cpp
  src/search/itinerary.cpp
  src/search/yen.cpp
  src/util/config.cpp
)
target_include_directories(safar_core PUBLIC src)
target_link_libraries(safar_core PUBLIC httplib::httplib nlohmann_json::nlohmann_json spdlog::spdlog)
target_compile_definitions(safar_core PUBLIC SAFAR_VERSION="${PROJECT_VERSION}")

if(MSVC)
  target_compile_options(safar_core PUBLIC /W4)
else()
  target_compile_options(safar_core PUBLIC -Wall -Wextra -Wpedantic)
endif()
if(SAFAR_WERROR AND NOT MSVC)
  target_compile_options(safar_core PUBLIC -Werror)
endif()
if(SAFAR_SANITIZE AND NOT MSVC)
  target_compile_options(safar_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(safar_core PUBLIC -fsanitize=address,undefined)
endif()

add_executable(safar_server src/main.cpp)
target_link_libraries(safar_server PRIVATE safar_core)

# ---- Tests ----
if(SAFAR_BUILD_TESTS)
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  # The fit_fares.py tests need a python3; they are skipped if none is found.
  find_package(Python3 COMPONENTS Interpreter)

  enable_testing()
  add_executable(safar_tests tests/test_geo.cpp tests/test_dataset.cpp tests/test_search.cpp tests/test_journey.cpp
                             tests/test_api.cpp tests/test_fares.cpp
                             tests/test_disruptions.cpp tests/test_optimize.cpp)
  target_link_libraries(safar_tests PRIVATE safar_core GTest::gtest_main)
  target_compile_definitions(safar_tests PRIVATE
    SAFAR_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data/processed"
    SAFAR_TEST_CONFIG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/config"
    SAFAR_SCRIPTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../scripts"
    SAFAR_PYTHON="${Python3_EXECUTABLE}")
  include(GoogleTest)
  gtest_discover_tests(safar_tests)
endif()
