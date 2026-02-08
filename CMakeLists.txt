cmake_minimum_required(VERSION 3.20)
project(flash_tool LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FLASH_TOOL_BUILD_TESTS "Build unit tests" OFF)

find_package(ZLIB REQUIRED)
find_package(LibArchive REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

add_library(flash_core
  src/fd.cpp
  src/file_reader.cpp
  src/partition_writer.cpp
  src/signals.cpp
  src/config_parser.cpp
  src/config_json_utils.cpp
  src/manifest.cpp
  src/manifest_parser.cpp
  src/version_comparator.cpp
  src/update_policy.cpp
  src/gzip_reader.cpp
  src/sha256.cpp
  src/update_module.cpp
  src/component_installers.cpp
  src/ota_bundle_reader.cpp
  src/logger.cpp
  src/ota_installer.cpp
  src/ota_install_services.cpp
  src/staging_verifier.cpp
  src/archive_installer.cpp
  src/progress_sinks.cpp
  src/archive_path_policy.cpp
  src/mount_session.cpp
  src/tar_stream_reader_adapter.cpp
  src/tar_stream_extractor.cpp
)

target_include_directories(flash_core PUBLIC include)
target_link_libraries(flash_core PUBLIC nlohmann_json::nlohmann_json ZLIB::ZLIB LibArchive::LibArchive OpenSSL::Crypto)

add_executable(flash_tool src/main.cpp)
target_link_libraries(flash_tool PRIVATE flash_core)

include(CTest)
if (FLASH_TOOL_BUILD_TESTS)
  enable_testing()
  find_package(GTest CONFIG REQUIRED)
  add_subdirectory(tests)
endif()
