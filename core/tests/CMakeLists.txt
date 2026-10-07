cmake_minimum_required(VERSION 3.10)

project(dazzle_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The state machine, its content and its wording are part of the library either way, so
# their tests run in both builds. The Markdown renderer is compiled only with the windows
# (see ../src/CMakeLists.txt), so its test goes with it.
set(DAZZLE_TEST_SOURCES
  test_support.cpp
  version_test.cpp
  ed25519_test.cpp
  release_index_parse_test.cpp
  updater_test.cpp
  updater_ui_test.cpp
  updater_ui_content_test.cpp
)
if(DAZZLE_WITH_UPDATER_UI)
  list(APPEND DAZZLE_TEST_SOURCES markdown_test.cpp)
endif()

add_executable(dazzle_tests ${DAZZLE_TEST_SOURCES})

target_link_libraries(dazzle_tests PRIVATE dazzle)

add_test(NAME dazzle_tests COMMAND dazzle_tests)

# A stand-in for the `dazzle sign` command specs/updater.md plans: it signs a package the
# way a release would, with the same Monocypher build the library verifies with.
if(APPLE)
  add_executable(fixture_signer fixture_signer.cpp)
  target_link_libraries(fixture_signer PRIVATE dazzle)

  # The end-to-end install test builds, replaces and relaunches application bundles, so
  # it is opt-in: ctest -R dazzle_macos_install_e2e
  add_test(NAME dazzle_macos_install_e2e
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/macos_install_e2e.sh"
            "$<TARGET_FILE:updater_example>" "$<TARGET_FILE:fixture_signer>")
  set_tests_properties(dazzle_macos_install_e2e PROPERTIES LABELS "e2e" TIMEOUT 180)
endif()
