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
  "name": "sweetedit_core",
  "id": "com.example.sweeteditcore",
  "target": "sweetedit_core"
}
]=])
endif ()
if (HUXERUI_PROJECT_PLAN_ONLY)
    return()
endif ()

project(sweetedit_core VERSION 0.1.0 LANGUAGES NONE)

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
if (NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    set(SWEETEDITOR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SWEETEDITOR_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(SWEETEDITOR_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SWEETEDITOR_BUILD_ANDROID_JNI OFF CACHE BOOL "" FORCE)
    set(SWEETEDITOR_BUILD_APPLE_FRAMEWORK OFF CACHE BOOL "" FORCE)
    set(SWEETEDITOR_BUILD_WASM_C_ABI OFF CACHE BOOL "" FORCE)
    set(SWEETEDITOR_BUILD_WASM_EMBIND OFF CACHE BOOL "" FORCE)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/3dparty/SweetEditor"
        "${CMAKE_BINARY_DIR}/sweeteditor"
        EXCLUDE_FROM_ALL
    )
endif ()

# SweetLine — third-party native syntax highlighting engine (C++17, vendored).
if (NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    set(SWEETLINE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SWEETLINE_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(SWEETLINE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SWEETLINE_BUILD_ANDROID_JNI OFF CACHE BOOL "" FORCE)
    set(SWEETLINE_BUILD_WASM_EMBIND OFF CACHE BOOL "" FORCE)
    set(SWEETLINE_BUILD_APPLE_FRAMEWORK OFF CACHE BOOL "" FORCE)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/3dparty/SweetLine"
        "${CMAKE_BINARY_DIR}/sweetline"
        EXCLUDE_FROM_ALL
    )
endif ()

file(GLOB_RECURSE LIBRARY_SOURCE_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
)

set(HUXERUI_LIBRARY_GRAPH_OUTPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/.huxerui/generated/libraries.json"
)

huxerui_add_library(sweetedit_core
        SOURCES
            ${LIBRARY_SOURCE_FILES}
        RESOURCES
            resources
        RESOURCE_NAMESPACE
            sweetedit_core
)

if (NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    target_link_libraries(
        sweetedit_core
        PRIVATE
            SweetEditor::sweeteditor_static
            SweetLine::sweetline_static
    )
endif ()

target_include_directories(sweetedit_core PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
)

add_library(SweeteditCore::SweeteditCore ALIAS sweetedit_core)
