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
  src/io/fd.cpp
  src/io/file_reader.cpp
  src/io/partition_writer.cpp
  src/io/gzip_reader.cpp
  src/system/signals.cpp
  src/util/config_parser.cpp
  src/util/config_json_utils.cpp
  src/util/device_config.cpp
  src/util/manifest.cpp
  src/util/manifest_parser.cpp
  src/util/manifest_selector.cpp
  src/util/version_comparator.cpp
  src/util/update_policy.cpp
  src/crypto/sha256.cpp
  src/ota/update_module.cpp
  src/ota/component_installers.cpp
  src/ota/ota_bundle_reader.cpp
  src/util/logger.cpp
  src/ota/ota_installer.cpp
  src/ota/ota_install_services.cpp
  src/ota/staging_verifier.cpp
  src/ota/archive_installer.cpp
  src/ota/progress_sinks.cpp
  src/ota/archive_path_policy.cpp
  src/ota/mount_session.cpp
  src/ota/tar_stream_reader_adapter.cpp
  src/ota/tar_stream_extractor.cpp
  src/util/console_progress.cpp
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
