cmake_minimum_required(VERSION 3.10)

project(updater_ui_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
  # The windows this example shows are AppKit's, so it is an Objective-C++ application
  # bundle: `enable_language` again here so the example can be configured on its own.
  enable_language(OBJCXX)

  add_executable(updater_ui_example MACOSX_BUNDLE main.mm)

  target_link_libraries(updater_ui_example PRIVATE dazzle_updater)

  # A bundle, because UpdaterUI asks the platform for the application's own icon and name,
  # and because the update this example demonstrates replaces a bundle in place. The
  # version here is the "current version" of the demo; demo.sh writes the feed URL and the
  # public key into the copy it runs.
  set_target_properties(updater_ui_example PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "DazzleUpdater UI Example"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.dazzle-ui-example"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
    # The update windows show the application's own icon, so the example ships one:
    # DazzleUpdaterDemo.icns, the dazzle updater brand mark on its tile
    # (branding/dazzle/dazzle-updater-app-icon.svg).
    MACOSX_BUNDLE_ICON_FILE "DazzleUpdaterDemo.icns"
  )

  set_source_files_properties(DazzleUpdaterDemo.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
  add_custom_command(TARGET updater_ui_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/DazzleUpdaterDemo.icns"
            "$<TARGET_BUNDLE_DIR:updater_ui_example>/Contents/Resources/DazzleUpdaterDemo.icns"
  )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # A GTK 4 application, and an application *directory* rather than a bundle: the update
  # this example demonstrates replaces the directory the binary sits in, which is what
  # demo_linux.sh builds around it. GTK comes in through the library, which links it when
  # the built-in windows are built; core/CMakeLists.txt only adds this directory when
  # GTK 4 was found.
  add_executable(updater_ui_example main_linux.cpp)

  target_link_libraries(updater_ui_example PRIVATE dazzle_updater)
endif()
