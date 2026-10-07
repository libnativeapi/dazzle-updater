# The language bindings build this library with whatever CMake the user has
# installed. Do not raise the minimum version without a reason; doing so
# breaks those builds for users on older CMake.
cmake_minimum_required(VERSION 3.10)

project(dazzle_updater_core VERSION 0.0.1 LANGUAGES CXX)

# Bindings that load the library at runtime (Dart FFI) need a shared library;
# bindings that link it (Rust) and the examples use the static one.
option(DAZZLE_BUILD_SHARED "Build dazzle_updater as a shared library" OFF)

# UpdaterUI's native windows (../../specs/updater-ui.md) are optional. OFF leaves the
# window and preferences implementations of this platform out of the build, together
# with the Markdown renderer and the system UI frameworks they need; the entry points
# and the C ABI stay, and answer false, exactly as they do on a platform that has no
# built-in windows. Nothing in the sources is conditioned on this option: it decides
# which file implements the two seams (see below), which is also why an application or a
# binding built either way links and runs.
option(DAZZLE_WITH_UPDATER_UI "Build the built-in UpdaterUI native windows" ON)

# Enable Objective-C++
if(APPLE)
    enable_language(OBJCXX)
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Common source files, minus the fallback implementations of the platform seams: those
# are added below, and only when the platform directory has no file for that seam.
file(GLOB COMMON_SOURCES "*.cpp" "foundation/*.cpp")
list(REMOVE_ITEM COMMON_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/foundation/http_default.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/foundation/system_default.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/foundation/updater_ui_backend_default.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/foundation/updater_ui_preferences_default.cpp")

# Vendored third-party code (core/third_party/, never edited locally): Monocypher
# provides Ed25519, nlohmann/json is a single header included by the release index
# parser. Both are compiled into the library and hidden from it (PRIVATE include).
set(THIRD_PARTY_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/monocypher/monocypher.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/monocypher/monocypher-ed25519.c"
)
set(THIRD_PARTY_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")

# C API source files
file(GLOB CAPI_SOURCES "capi/*.cpp" "capi/*.c")

# Platform-specific source files. ANDROID and iOS must be checked before the
# generic APPLE branch, or they fall through to the desktop implementation.
if(ANDROID)
    set(PLATFORM_DIR "platform/android")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(PLATFORM_DIR "platform/ios")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PLATFORM_DIR "platform/linux")
elseif(APPLE)
    set(PLATFORM_DIR "platform/macos")
elseif(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    set(PLATFORM_DIR "platform/ohos")
elseif(WIN32)
    set(PLATFORM_DIR "platform/windows")
else()
    set(PLATFORM_DIR "")
endif()

set(PLATFORM_SOURCES "")
if(PLATFORM_DIR)
    file(GLOB PLATFORM_SOURCES "${PLATFORM_DIR}/*.cpp" "${PLATFORM_DIR}/*.mm")
endif()

# Linux brings two optional dependencies of its own, and both are found the way Linux
# finds libraries: libcurl for the HTTP seam, GTK 4 for the built-in windows. Neither is
# required — without libcurl the HTTP seam falls back to the cross-platform default (and
# system_linux.cpp then reports that this build cannot check or download), without GTK 4
# the cross-platform backend answers that there are no built-in windows. So a Linux build
# with neither still compiles and links, which is what the bindings rely on.
set(DAZZLE_LINUX_HTTP_AVAILABLE OFF)
set(DAZZLE_LINUX_GTK4_AVAILABLE OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBCURL QUIET IMPORTED_TARGET libcurl)
        if(LIBCURL_FOUND)
            set(DAZZLE_LINUX_HTTP_AVAILABLE ON)
        endif()
        pkg_check_modules(GTK4 QUIET IMPORTED_TARGET gtk4)
        if(GTK4_FOUND)
            set(DAZZLE_LINUX_GTK4_AVAILABLE ON)
        endif()
    endif()
endif()
# Published as cache entries, because core/CMakeLists.txt decides whether to build the
# example from them and a subdirectory's normal variables do not reach their parent.
set(DAZZLE_LINUX_HTTP_AVAILABLE ${DAZZLE_LINUX_HTTP_AVAILABLE} CACHE INTERNAL
    "Linux: libcurl was found (HTTP seam)" FORCE)
set(DAZZLE_LINUX_GTK4_AVAILABLE ${DAZZLE_LINUX_GTK4_AVAILABLE} CACHE INTERNAL
    "Linux: GTK 4 was found (built-in windows)" FORCE)

# Windows brings one optional dependency of its own, found where the Microsoft.WindowsAppSDK
# NuGet package leaves it: the C++/WinRT projections and libraries the built-in windows are
# compiled against. Without them the cross-platform backend answers that there are no
# built-in windows, exactly as on Linux without GTK 4, so a Windows build with no SDK
# restored still compiles and links.
set(DAZZLE_WINDOWS_WINUI3_AVAILABLE OFF)
set(DAZZLE_WASDK_INCLUDE_DIRS "")
set(DAZZLE_WASDK_LIB_DIRS "")
set(DAZZLE_WASDK_METADATA_DIRS "")
set(DAZZLE_UNION_WINMD "")
if(WIN32)
    # The Windows App SDK is a metapackage: the payload lives in component packages, each
    # with its winmd and its native libraries, and C++/WinRT's projections are generated
    # from those winmds rather than shipped as headers. cppwinrt comes with the Windows
    # SDK, which is also where the reference winmd is.
    file(TO_CMAKE_PATH "$ENV{USERPROFILE}" DAZZLE_USERPROFILE)
    # The Windows SDK lives under Program Files (x86); the environment variable's name has
    # parentheses, and a literal path is what every Windows install has anyway.
    set(DAZZLE_X86 "C:/Program Files (x86)")
    set(DAZZLE_NUGET "${DAZZLE_USERPROFILE}/.nuget/packages")
    foreach(component
            microsoft.windowsappsdk.foundation
            microsoft.windowsappsdk.interactiveexperiences
            microsoft.windowsappsdk.winui)
        file(GLOB versions "${DAZZLE_NUGET}/${component}/1.8.*")
        if(NOT versions)
            continue()
        endif()
        list(SORT versions)
        list(GET versions -1 version)
        if(IS_DIRECTORY "${version}/include")
            list(APPEND DAZZLE_WASDK_INCLUDE_DIRS "${version}/include")
        endif()
        if(IS_DIRECTORY "${version}/lib/native/x64")
            list(APPEND DAZZLE_WASDK_LIB_DIRS "${version}/lib/native/x64")
        endif()
        if(IS_DIRECTORY "${version}/metadata")
            # One directory per component: the per-TFM one when it holds winmds (those are
            # the variant for the platform we build), otherwise metadata/ itself. Handing
            # cppwinrt both would project the same namespaces twice.
            file(GLOB tfm_dirs "${version}/metadata/10.0.*")
            set(chosen "")
            foreach(tfm_dir ${tfm_dirs})
                file(GLOB tfm_winmds "${tfm_dir}/*.winmd")
                if(tfm_winmds)
                    set(chosen "${tfm_dir}")
                endif()
            endforeach()
            if(NOT chosen)
                file(GLOB metadata_winmds "${version}/metadata/*.winmd")
                if(metadata_winmds)
                    set(chosen "${version}/metadata")
                endif()
            endif()
            if(chosen)
                list(APPEND DAZZLE_WASDK_METADATA_DIRS "${chosen}")
            endif()
        endif()
    endforeach()
    # Microsoft.UI.Xaml.winmd names WebView2 types; the projections cannot be generated
    # without its winmd even though the windows here use none of them.
    # The package puts it in lib/ (not lib/native/), next to the native headers.
    file(GLOB DAZZLE_WEBVIEW2_WINMDS
        "${DAZZLE_NUGET}/microsoft.web.webview2/*/lib/*.winmd"
        "${DAZZLE_NUGET}/microsoft.web.webview2/*/lib/native/*.winmd"
        "${DAZZLE_NUGET}/microsoft.web.webview2/*/build/native/*.winmd")
    if(DAZZLE_WEBVIEW2_WINMDS)
        list(GET DAZZLE_WEBVIEW2_WINMDS 0 DAZZLE_WEBVIEW2_WINMD)
        list(APPEND DAZZLE_WASDK_METADATA_DIRS "${DAZZLE_WEBVIEW2_WINMD}")
    endif()
    # C++/WinRT's generator and its own winrt/ headers ship together in one package, and
    # that pair is what the projections are meant to be compiled against. Prefer it over
    # the SDK's generator and the SDK's headers, which are not necessarily the same
    # generation — mixing them is what makes MSVC report that IVector::Append "returns auto
    # and must be defined first" for an ordinary call.
    file(GLOB_RECURSE DAZZLE_CPPWINRT_NUGET
        "${DAZZLE_NUGET}/microsoft.windows.cppwinrt/*/bin/cppwinrt.exe")
    if(DAZZLE_CPPWINRT_NUGET)
        list(SORT DAZZLE_CPPWINRT_NUGET)
        list(GET DAZZLE_CPPWINRT_NUGET -1 DAZZLE_CPPWINRT_NUGET_EXE)
        get_filename_component(DAZZLE_CPPWINRT_BIN "${DAZZLE_CPPWINRT_NUGET_EXE}" DIRECTORY)
        get_filename_component(DAZZLE_CPPWINRT_ROOT "${DAZZLE_CPPWINRT_BIN}" DIRECTORY)
        file(GLOB_RECURSE DAZZLE_CPPWINRT_BASES "${DAZZLE_CPPWINRT_ROOT}/*/winrt/base.h")
        if(DAZZLE_CPPWINRT_BASES)
            # The package normally carries the generator only, in which case the SDK's pair
            # is the one to use: its generator and its headers belong together.
            list(GET DAZZLE_CPPWINRT_BASES 0 DAZZLE_CPPWINRT_BASE)
            get_filename_component(DAZZLE_CPPWINRT_INCLUDE "${DAZZLE_CPPWINRT_BASE}" DIRECTORY)
            get_filename_component(DAZZLE_CPPWINRT_INCLUDE "${DAZZLE_CPPWINRT_INCLUDE}" DIRECTORY)
            set(DAZZLE_CPPWINRT "${DAZZLE_CPPWINRT_NUGET_EXE}")
            message(STATUS "C++/WinRT from NuGet: ${DAZZLE_CPPWINRT} headers ${DAZZLE_CPPWINRT_INCLUDE}")
        endif()
    endif()

    # The generated code and winrt/base.h have to be the same generation: the newest SDK's
    # own projections go on the include path, ahead of whatever the environment offers.
    file(GLOB DAZZLE_SDK_INCLUDES "${DAZZLE_X86}/Windows Kits/10/Include/10.0.*")
    if(DAZZLE_SDK_INCLUDES)
        list(SORT DAZZLE_SDK_INCLUDES)
        list(GET DAZZLE_SDK_INCLUDES -1 DAZZLE_SDK_INCLUDE)
        list(APPEND DAZZLE_WASDK_INCLUDE_DIRS "${DAZZLE_SDK_INCLUDE}/cppwinrt")
    endif()
    file(GLOB DAZZLE_UNION_CANDIDATES "${DAZZLE_X86}/Windows Kits/10/UnionMetadata/10.0.*")
    if(DAZZLE_UNION_CANDIDATES)
        list(SORT DAZZLE_UNION_CANDIDATES)
        list(GET DAZZLE_UNION_CANDIDATES -1 DAZZLE_UNION_DIR)
        set(DAZZLE_UNION_WINMD "${DAZZLE_UNION_DIR}/Windows.winmd")
    endif()
    # cppwinrt's generated code and the winrt/base.h it is compiled against have to be the
    # same generation: the newest SDK's generator, because that SDK's headers are the ones
    # the toolchain puts on the include path.
    file(GLOB DAZZLE_CPPWINRT_CANDIDATES "${DAZZLE_X86}/Windows Kits/10/bin/10.0.*/x64/cppwinrt.exe")
    if(DAZZLE_CPPWINRT_CANDIDATES)
        list(SORT DAZZLE_CPPWINRT_CANDIDATES)
        list(GET DAZZLE_CPPWINRT_CANDIDATES -1 DAZZLE_CPPWINRT)
    else()
        find_program(DAZZLE_CPPWINRT cppwinrt)
    endif()
    if(DAZZLE_WASDK_METADATA_DIRS AND DAZZLE_UNION_WINMD AND DAZZLE_CPPWINRT)
        set(DAZZLE_WINDOWS_WINUI3_AVAILABLE ON)
    endif()
endif()
set(DAZZLE_WINDOWS_WINUI3_AVAILABLE ${DAZZLE_WINDOWS_WINUI3_AVAILABLE} CACHE INTERNAL
    "Windows: the Windows App SDK payload was found (built-in windows)" FORCE)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT DAZZLE_LINUX_HTTP_AVAILABLE)
    list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "http_linux")
endif()
if(WIN32 AND NOT DAZZLE_WINDOWS_WINUI3_AVAILABLE)
    list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "updater_ui_backend_winui3")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT DAZZLE_LINUX_GTK4_AVAILABLE)
    # The GTK 4 windows and the Markdown renderer they use; the preference store is a
    # plain file and stays, so a build without GTK still remembers the user's answers.
    list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "updater_ui_backend_linux")
    list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "updater_ui_notes_linux")
endif()

# A build without the built-in UI drops this platform's windows and its preference store,
# so that the same fallbacks below answer for it. The globs above pick every file up by
# extension, so this is the only place the option has to be considered.
if(NOT DAZZLE_WITH_UPDATER_UI)
    list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "updater_ui_")
else()
    # Markdown for the notes box: the parser is vendored (third_party/md4c), the writers
    # are dazzle_updater's own. Only the windows render Markdown, so only they pull it in.
    file(GLOB MARKDOWN_SOURCES "markdown/*.cpp")
    list(APPEND COMMON_SOURCES ${MARKDOWN_SOURCES})
    list(APPEND THIRD_PARTY_SOURCES
      "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/md4c/md4c.c")
endif()

# Every platform seam has a cross-platform default, and a platform overrides it by
# providing <seam>_<os> in its directory. So a platform needs source files only for the
# behavior it really implements: Android, iOS and OHOS carry none today, and Windows and
# Linux keep only what they already support.
set(SEAM_FALLBACK_SOURCES "")
if(NOT "${PLATFORM_SOURCES}" MATCHES "http_")
    list(APPEND SEAM_FALLBACK_SOURCES "foundation/http_default.cpp")
endif()
if(NOT "${PLATFORM_SOURCES}" MATCHES "system_")
    list(APPEND SEAM_FALLBACK_SOURCES "foundation/system_default.cpp")
endif()
# UpdaterUI's two seams: the windows, and what the user answered. A platform without
# either gets the cross-platform default, which is also what DAZZLE_WITH_UPDATER_UI=OFF
# ends up compiling for every platform.
if(NOT "${PLATFORM_SOURCES}" MATCHES "updater_ui_backend_")
    list(APPEND SEAM_FALLBACK_SOURCES "foundation/updater_ui_backend_default.cpp")
endif()
if(NOT "${PLATFORM_SOURCES}" MATCHES "updater_ui_preferences_")
    list(APPEND SEAM_FALLBACK_SOURCES "foundation/updater_ui_preferences_default.cpp")
endif()

# The macOS windows and their preference store are written for ARC; the older
# Objective-C++ files here manage their objects by hand. Set per source file, so the flag
# reaches neither the vendored C nor anything else.
if(APPLE AND DAZZLE_WITH_UPDATER_UI)
    set_source_files_properties(
      "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/updater_ui_backend_macos.mm"
      "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/updater_ui_controls_macos.mm"
      "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/updater_ui_preferences_macos.mm"
      PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endif()

# Add library target
if(DAZZLE_BUILD_SHARED)
  set(DAZZLE_LIBRARY_TYPE SHARED)
else()
  set(DAZZLE_LIBRARY_TYPE STATIC)
endif()

add_library(dazzle_updater ${DAZZLE_LIBRARY_TYPE}
  ${COMMON_SOURCES}
  ${PLATFORM_SOURCES}
  ${CAPI_SOURCES}
  ${THIRD_PARTY_SOURCES}
  ${SEAM_FALLBACK_SOURCES}
)

# Propagate the language requirement to anything that links dazzle_updater; the
# CMAKE_CXX_STANDARD variables above only govern this directory's targets.
target_compile_features(dazzle_updater PUBLIC cxx_std_17)

# Set library include directories
target_include_directories(dazzle_updater PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  $<INSTALL_INTERFACE:include>
)

# The vendored headers stay an implementation detail of the library.
target_include_directories(dazzle_updater PRIVATE "${THIRD_PARTY_INCLUDE_DIR}")


# Link required frameworks and libraries
if(ANDROID)
    target_link_libraries(dazzle_updater PUBLIC log android)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_link_libraries(dazzle_updater PUBLIC "-framework Foundation")
    target_compile_options(dazzle_updater PRIVATE "-x" "objective-c++")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(dazzle_updater PUBLIC pthread)
    # libcurl carries the HTTP seam; the include directories and link flags come with the
    # pkg-config imported target, which is also what makes the build work from a prefix
    # (PKG_CONFIG_PATH) rather than from /usr.
    if(DAZZLE_LINUX_HTTP_AVAILABLE)
        target_link_libraries(dazzle_updater PUBLIC PkgConfig::LIBCURL)
    endif()
    # The windows are GTK 4's; a build without them links nothing of it.
    if(DAZZLE_WITH_UPDATER_UI AND DAZZLE_LINUX_GTK4_AVAILABLE)
        target_link_libraries(dazzle_updater PUBLIC PkgConfig::GTK4)
    endif()
elseif(APPLE)
    target_link_libraries(dazzle_updater PUBLIC "-framework Foundation")
    # The Objective-C++ files are the platform ones, and the `.mm` extension is already
    # what makes CMake compile them as OBJCXX (enable_language above). The vendored C
    # files are therefore left as C: md4c does not compile as Objective-C++.
    # The windows are AppKit; a build without them does not link it at all.
    if(DAZZLE_WITH_UPDATER_UI)
        target_link_libraries(dazzle_updater PUBLIC "-framework AppKit")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    target_link_libraries(dazzle_updater PUBLIC hilog_ndk.z)
elseif(WIN32)
    # WinHTTP for the HTTP seam, version.lib for the executable's VERSIONINFO, shell32 for
    # the installer launch (system_windows.cpp).
    target_link_libraries(dazzle_updater PUBLIC kernel32 winhttp version shell32)
    # C++/WinRT wants C++20 (or /await), and the projections come from the Windows App SDK
    # payload. Only the backend files take the newer mode: the rest of core is C++17.
    if(DAZZLE_WITH_UPDATER_UI AND DAZZLE_WINDOWS_WINUI3_AVAILABLE)
        # The projections the backend includes are generated once, into the build tree.
        set(DAZZLE_WINRT_GENERATED "${CMAKE_BINARY_DIR}/winrt-generated")
        set(DAZZLE_WINRT_STAMP "${DAZZLE_WINRT_GENERATED}/winrt/Microsoft.UI.Xaml.h")
        set(DAZZLE_WINRT_LOG "${CMAKE_BINARY_DIR}/winrt-generated.log")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/generate_winrt.cmake.in"
                       "${CMAKE_BINARY_DIR}/generate-winrt.cmake" @ONLY)
        add_custom_command(
            OUTPUT "${DAZZLE_WINRT_STAMP}"
            COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/generate-winrt.cmake"
            COMMENT "Generating the C++/WinRT projections for the Windows App SDK"
            VERBATIM)
        add_custom_target(dazzle_updater_winrt_projections DEPENDS "${DAZZLE_WINRT_STAMP}")
        add_dependencies(dazzle_updater dazzle_updater_winrt_projections)
        foreach(source ${PLATFORM_SOURCES})
            if(source MATCHES "updater_ui_backend_winui3")
                set_source_files_properties("${source}" PROPERTIES
                    COMPILE_OPTIONS "/std:c++17;/await;/permissive;/EHsc;/bigobj;/D_WIN32_WINNT=0x0A00")
            endif()
        endforeach()
        target_include_directories(dazzle_updater PRIVATE
            "${DAZZLE_WINRT_GENERATED}" ${DAZZLE_WASDK_INCLUDE_DIRS})
        if(DAZZLE_CPPWINRT_INCLUDE)
            # Ahead of everything: the matching base.h decides whether the projections
            # compile.
            target_include_directories(dazzle_updater BEFORE PRIVATE "${DAZZLE_CPPWINRT_INCLUDE}")
        endif()
        # C++/WinRT's own entry points (RoGetActivationFactory, RoOriginateLanguageException)
        # come from the Windows Runtime umbrella library.
        target_link_libraries(dazzle_updater PRIVATE windowsapp)
        foreach(lib_dir ${DAZZLE_WASDK_LIB_DIRS})
            file(GLOB component_libs "${lib_dir}/*.lib")
            target_link_libraries(dazzle_updater PRIVATE ${component_libs})
        endforeach()
    endif()
    # Native icon extraction and the message window used to dispatch onto the XAML
    # thread. WinUI itself supplies the controls, theme and UI Automation support.
    if(DAZZLE_WITH_UPDATER_UI AND DAZZLE_WINDOWS_WINUI3_AVAILABLE)
        target_link_libraries(dazzle_updater PRIVATE user32 gdi32)
    endif()
endif()
