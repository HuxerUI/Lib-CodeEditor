cmake_minimum_required(VERSION 3.20)

if (HUXERUI_PROJECT_PLAN_OUTPUT)
    get_filename_component(HUXERUI_PROJECT_PLAN_DIRECTORY
            "${HUXERUI_PROJECT_PLAN_OUTPUT}"
            DIRECTORY
    )
    file(MAKE_DIRECTORY "${HUXERUI_PROJECT_PLAN_DIRECTORY}")
    file(WRITE "${HUXERUI_PROJECT_PLAN_OUTPUT}" [=[
{
  "schema": 1,
  "kind": "library",
  "name": "codeeditor",
  "id": "com.example.codeeditor",
  "target": "codeeditor"
}
]=])
endif ()
if (HUXERUI_PROJECT_PLAN_ONLY)
    return()
endif ()

project(codeeditor VERSION 0.1.0 LANGUAGES NONE)

enable_language(CXX)

if (NOT TARGET HuxerUI::huxerui)
    set(HUXERUI_HOME "$ENV{HUXERUI_HOME}" CACHE PATH "HuxerUI SDK or source directory")
    set(HUXERUI_HOST_TOOL_ROOT "$ENV{HUXERUI_HOST_TOOL_ROOT}" CACHE PATH "HuxerUI host tool root")
    if (HUXERUI_HOME AND EXISTS "${HUXERUI_HOME}/CMakeLists.txt"
            AND EXISTS "${HUXERUI_HOME}/include/huxerui/huxerui.h")
        set(HUXERUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(HUXERUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        if (ANDROID)
            set(HUXERUI_BUILD_SHARED ON CACHE BOOL "" FORCE)
            set(HUXERUI_BUILD_STATIC OFF CACHE BOOL "" FORCE)
        else ()
            set(HUXERUI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
            set(HUXERUI_BUILD_STATIC ON CACHE BOOL "" FORCE)
        endif ()
        add_subdirectory("${HUXERUI_HOME}" "${CMAKE_BINARY_DIR}/huxerui-sdk" EXCLUDE_FROM_ALL)
    else ()
        if (HUXERUI_HOME)
            find_package(HuxerUI CONFIG REQUIRED
                    PATHS "${HUXERUI_HOME}"
                    NO_DEFAULT_PATH
                    NO_CMAKE_FIND_ROOT_PATH
            )
        else ()
            find_package(HuxerUI CONFIG REQUIRED)
        endif ()
    endif ()
endif ()

# SweetEditor — third-party cross-platform editor core (C++17, vendored).
set(SWEETEDITOR_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3dparty/SweetEditor/include")
if (NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    include(FetchContent)
    set(SWEETEDITOR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SWEETEDITOR_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(SWEETEDITOR_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SWEETEDITOR_BUILD_ANDROID_JNI OFF CACHE BOOL "" FORCE)
    set(SWEETEDITOR_BUILD_APPLE_FRAMEWORK OFF CACHE BOOL "" FORCE)
    set(SWEETEDITOR_BUILD_WASM_C_ABI OFF CACHE BOOL "" FORCE)
    set(SWEETEDITOR_BUILD_WASM_EMBIND OFF CACHE BOOL "" FORCE)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3dparty/SweetEditor/CMakeLists.txt")
        add_subdirectory(
            "${CMAKE_CURRENT_SOURCE_DIR}/3dparty/SweetEditor"
            "${CMAKE_BINARY_DIR}/sweeteditor"
            EXCLUDE_FROM_ALL
        )
    else ()
        FetchContent_Declare(
            sweeteditor_source
            GIT_REPOSITORY https://github.com/FinalScave/SweetEditor.git
            GIT_TAG bd4330e70eda5c71b71ecac54cf7312d82d2eae2
            GIT_SHALLOW FALSE
            # logging.hpp routes Apple logging through os/log.h, which
            # hard-errors under GCC, so the patch restricts it to clang.
            # Nested 3dparty checkouts must carry the same patch (see
            # 3dparty/patches/).
            PATCH_COMMAND bash -c "git apply -R --check '${CMAKE_CURRENT_LIST_DIR}/3dparty/patches/sweeteditor-oslog-gcc.patch' 2>/dev/null || git apply '${CMAKE_CURRENT_LIST_DIR}/3dparty/patches/sweeteditor-oslog-gcc.patch'"
        )
        FetchContent_MakeAvailable(sweeteditor_source)
        # The public header includes <sweeteditor/decoration.h>; point
        # consumers at the fetched copy instead of the empty vendored dir.
        set(SWEETEDITOR_INCLUDE_DIR "${sweeteditor_source_SOURCE_DIR}/include")
    endif ()
endif ()

# SweetLine is intentionally NOT a dependency: highlighting plugs in through
# the CodeEditorDecorationProvider interface (see examples/preview for the
# optional SweetLine integration).

file(GLOB_RECURSE LIBRARY_SOURCE_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
)

set(HUXERUI_LIBRARY_GRAPH_OUTPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/.huxerui/generated/libraries.json"
)

huxerui_add_library(codeeditor
        SOURCES
            ${LIBRARY_SOURCE_FILES}
)

# windows.h defines min/max macros (breaking std::min/std::max in the vendored
# editor headers) and, through rpcndr.h, a `small` macro (breaking theme.h
# fields such as `float small`). NOMINMAX keeps the std versions intact and
# WIN32_LEAN_AND_MEAN keeps rpcndr.h out; both match upstream configurations.
if (WIN32)
    target_compile_definitions(codeeditor PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif ()

# ShouldCommitFocusOnPointerUp is a newer HuxerUI NodeExtension hook (keeps
# editor drags and command taps from raising the keyboard) that released SDKs
# may not ship. Detect it so the component overrides it only when available.
# ScrollInputEvent likewise exists only in newer SDKs; scan every header so
# one hook's match cannot hide the other.
set(HUXERUI_HAS_FOCUS_POINTER_HOOK OFF)
set(HUXERUI_HAS_SCROLL_INPUT_EVENT OFF)
if (HUXERUI_HOME AND IS_DIRECTORY "${HUXERUI_HOME}/include/huxerui")
    file(GLOB HUXERUI_PUBLIC_HEADERS "${HUXERUI_HOME}/include/huxerui/*.h")
    foreach (HUXERUI_HEADER ${HUXERUI_PUBLIC_HEADERS})
        file(READ "${HUXERUI_HEADER}" HUXERUI_HEADER_CONTENT)
        if (HUXERUI_HEADER_CONTENT MATCHES "ShouldCommitFocusOnPointerUp")
            set(HUXERUI_HAS_FOCUS_POINTER_HOOK ON)
        endif ()
        if (HUXERUI_HEADER_CONTENT MATCHES "struct ScrollInputEvent")
            set(HUXERUI_HAS_SCROLL_INPUT_EVENT ON)
        endif ()
    endforeach ()
endif ()
if (HUXERUI_HAS_FOCUS_POINTER_HOOK)
    target_compile_definitions(
        codeeditor
        PRIVATE
            CODEEDITOR_HAS_SHOULD_COMMIT_FOCUS_ON_POINTER_UP
    )
endif ()
if (HUXERUI_HAS_SCROLL_INPUT_EVENT)
    target_compile_definitions(codeeditor PRIVATE CODEEDITOR_HAS_SCROLL_INPUT_EVENT)
endif ()

if (NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    target_link_libraries(
        codeeditor
        PRIVATE
            SweetEditor::sweeteditor_static
    )
    if (ANDROID)
        # android/log.h diagnostics for on-device tracing.
        target_link_libraries(codeeditor PRIVATE log)
    endif ()
endif ()

target_include_directories(codeeditor PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${SWEETEDITOR_INCLUDE_DIR}>"
        "$<INSTALL_INTERFACE:include>"
)

add_library(HuxerUI::CodeEditor ALIAS codeeditor)

# Component tests exercise pure public-API logic (options validation,
# disconnected controller behavior, theme derivation). They require the
# Catch2 amalgamation shipped with a source HuxerUI checkout; an installed
# SDK build skips them.
set(HUXERUI_CODEEDITOR_DEFAULT_TESTS OFF)
if (NOT HUXERUI_LIBRARY_GRAPH_ONLY AND NOT EMSCRIPTEN AND NOT IOS AND NOT ANDROID
        AND HUXERUI_HOME AND TARGET HuxerUI::huxerui_static
        AND EXISTS "${HUXERUI_HOME}/3dparty/catch2/huxerui_3p.cmake")
    set(HUXERUI_CODEEDITOR_DEFAULT_TESTS ON)
endif ()
option(HUXERUI_CODEEDITOR_BUILD_TESTS "Build CodeEditor component tests" ${HUXERUI_CODEEDITOR_DEFAULT_TESTS})
if (HUXERUI_CODEEDITOR_BUILD_TESTS)
    enable_testing()
    include("${HUXERUI_HOME}/3dparty/catch2/huxerui_3p.cmake")
    add_executable(codeeditor_tests
            tests/test_main.cpp
            tests/test_options_validation.cpp
            tests/test_editor_controller.cpp
            tests/test_editor_theme.cpp
    )
    target_link_libraries(codeeditor_tests
            PRIVATE
                codeeditor
                HuxerUI::huxerui_static
                huxerui_3p_catch2
    )
    add_test(NAME codeeditor_tests COMMAND codeeditor_tests)
endif ()
