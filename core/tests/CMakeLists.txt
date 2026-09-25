cmake_minimum_required(VERSION 3.10)

project(dazzle_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(dazzle_tests
  test_support.cpp
  version_test.cpp
  ed25519_test.cpp
  release_index_parse_test.cpp
  updater_test.cpp
)

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
