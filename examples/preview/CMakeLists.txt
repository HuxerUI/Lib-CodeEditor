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
  "kind": "app",
  "name": "CodeEditor Preview",
  "id": "com.example.codeeditor.preview",
  "target": "example_codeeditor"
}
]=])
endif ()
if (HUXERUI_PROJECT_PLAN_ONLY)
    return()
endif ()

project(example_codeeditor VERSION 0.1.0 LANGUAGES NONE)

if (NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    enable_language(CXX)
endif ()

# Termux builds pass a source-change stamp so AGP re-runs the configure step
# (and refreshes staged library sources) whenever library sources change.
set(CODEEDITOR_SOURCE_STAMP "" CACHE STRING "Termux source change stamp")

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

if (WIN32)
    include("${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/huxerui.cmake" OPTIONAL)
elseif (APPLE AND NOT IOS)
    include("${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/huxerui.cmake" OPTIONAL)
elseif (EMSCRIPTEN)
    include("${CMAKE_CURRENT_SOURCE_DIR}/platform/web/huxerui.cmake" OPTIONAL)
endif ()

# SweetLine powers the demo's optional syntax highlighting through the
# CodeEditorDecorationProvider interface; the editor library itself never
# depends on it.
if (NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    set(SWEETLINE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SWEETLINE_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(SWEETLINE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SWEETLINE_BUILD_ANDROID_JNI OFF CACHE BOOL "" FORCE)
    set(SWEETLINE_BUILD_WASM_EMBIND OFF CACHE BOOL "" FORCE)
    set(SWEETLINE_BUILD_APPLE_FRAMEWORK OFF CACHE BOOL "" FORCE)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../3dparty/SweetLine"
        "${CMAKE_BINARY_DIR}/sweetline"
        EXCLUDE_FROM_ALL
    )
endif ()

file(GLOB_RECURSE APP_SOURCE_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
)
if (NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    if (WIN32)
        list(APPEND APP_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/main.cpp")
    elseif (APPLE AND NOT IOS)
        list(APPEND APP_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/main.cpp")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND APP_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/main.cpp")
    endif ()
endif ()

set(HUXERUI_LIBRARY_GRAPH_OUTPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/.huxerui/generated/libraries.json"
)

set(HUXERUI_APP_RESOURCE_OUTPUT_ARGUMENTS)
if (ANDROID AND HUXERUI_ANDROID_RESOURCE_OUTPUT_ROOT)
    list(APPEND HUXERUI_APP_RESOURCE_OUTPUT_ARGUMENTS
            RESOURCE_OUTPUT_DIRECTORY
            "${HUXERUI_ANDROID_RESOURCE_OUTPUT_ROOT}/${ANDROID_ABI}"
    )
endif ()

huxerui_add_app(example_codeeditor
        SOURCES
            ${APP_SOURCE_FILES}
        RESOURCES
            resources
        RESOURCE_NAMESPACE
            app
        ${HUXERUI_APP_RESOURCE_OUTPUT_ARGUMENTS}
        BUNDLE_NAME
            "CodeEditor Preview"
        BUNDLE_IDENTIFIER
            "com.example.codeeditor.preview"
)

# windows.h defines min/max macros (breaking std::min/std::max in the editor
# headers) and, through rpcndr.h, a `small` macro (breaking theme.h fields such
# as `float small`). NOMINMAX keeps the std versions intact and
# WIN32_LEAN_AND_MEAN keeps rpcndr.h out; both match upstream configurations.
if (WIN32 AND NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    target_compile_definitions(example_codeeditor PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif ()

if (WIN32 AND HUXERUI_WINDOWS_MANIFEST)
    target_sources(example_codeeditor PRIVATE "${HUXERUI_WINDOWS_MANIFEST}")
endif ()
if (APPLE AND NOT IOS AND HUXERUI_MACOS_INFO_PLIST)
    set_target_properties(example_codeeditor PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${HUXERUI_MACOS_INFO_PLIST}"
    )
endif ()
if (EMSCRIPTEN AND COMMAND huxerui_configure_web_app)
    huxerui_configure_web_app(example_codeeditor)
endif ()

# The editor component library lives in the repository root.
if (NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    target_link_libraries(
        example_codeeditor
        PRIVATE
            SweetLine::sweetline_static
    )
endif ()

huxerui_use_library(example_codeeditor
        TARGET CodeEditor::CodeEditor
        PATH "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)
