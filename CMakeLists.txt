cmake_minimum_required(VERSION 3.20)
project(dino_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DINO_GODOT "Build Godot 4.6 GDExtension (fetches godot-cpp)" OFF)
# No godot-cpp 4.6 tag exists. 4.4-stable matches godot/dino_sim.gdextension
# compatibility_minimum; the pinned 4.6 editor loads it.
set(DINO_GODOT_CPP_TAG "godot-4.4-stable" CACHE STRING "godot-cpp git tag")
set(GODOT_EXECUTABLE "" CACHE FILEPATH "Godot 4.6 editor (tools/godot/bin after scripts/setup_godot)")

add_library(dino STATIC
    src/sim.cpp
    src/perceive.cpp
    src/scripted.cpp
    src/feel.cpp
    src/golden.cpp
)
target_include_directories(dino PUBLIC ${CMAKE_SOURCE_DIR}/include)

if(MSVC)
    target_compile_options(dino PRIVATE /W4 $<$<CONFIG:Release>:/O2 /arch:AVX2>)
else()
    target_compile_options(dino PRIVATE -Wall -Wextra -Wpedantic $<$<CONFIG:Release>:-O3 -march=native>)
endif()

add_library(dino_c SHARED src/c_api.cpp)
target_link_libraries(dino_c PRIVATE dino)
target_include_directories(dino_c PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(dino_c PRIVATE DINO_C_EXPORTS)
set_target_properties(dino_c PROPERTIES OUTPUT_NAME dino_c)
if(MSVC)
    target_compile_options(dino_c PRIVATE $<$<CONFIG:Release>:/O2 /arch:AVX2>)
else()
    target_compile_options(dino_c PRIVATE $<$<CONFIG:Release>:-O3 -march=native>)
endif()
if(WIN32)
    set_target_properties(dino_c PROPERTIES PREFIX "")
endif()
if(MINGW)
    target_link_libraries(dino_c PRIVATE -static-libgcc -static-libstdc++ -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic)
    get_filename_component(_dino_mingw_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    if(EXISTS "${_dino_mingw_bin}/libwinpthread-1.dll")
        add_custom_command(TARGET dino_c POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_dino_mingw_bin}/libwinpthread-1.dll"
                "$<TARGET_FILE_DIR:dino_c>/libwinpthread-1.dll")
    endif()
endif()

add_executable(dino_tests src/dino_tests.cpp)
target_link_libraries(dino_tests PRIVATE dino)

add_executable(dino_parity src/dino_parity.cpp)
target_link_libraries(dino_parity PRIVATE dino)

add_executable(dino_viz src/dino_viz.cpp)
target_link_libraries(dino_viz PRIVATE dino)

enable_testing()
add_test(NAME dino_unit_tests COMMAND dino_tests)

if(NOT GODOT_EXECUTABLE AND EXISTS "${CMAKE_SOURCE_DIR}/tools/godot/editor.path")
    file(STRINGS "${CMAKE_SOURCE_DIR}/tools/godot/editor.path" _dino_godot_path LIMIT_COUNT 1)
    if(EXISTS "${_dino_godot_path}")
        set(GODOT_EXECUTABLE "${_dino_godot_path}" CACHE FILEPATH "Godot 4.6 editor" FORCE)
    endif()
endif()

if(NOT GODOT_EXECUTABLE)
    if(WIN32)
        set(_dino_godot_candidates
            "${CMAKE_SOURCE_DIR}/tools/godot/bin/Godot_v4.6-stable_win64_console.exe"
            "${CMAKE_SOURCE_DIR}/tools/godot/bin/Godot_v4.6-stable_win64.exe"
            "${CMAKE_SOURCE_DIR}/tools/godot/bin/Godot_v4.6-stable_windows_arm64_console.exe"
        )
    elseif(APPLE)
        set(_dino_godot_candidates
            "${CMAKE_SOURCE_DIR}/tools/godot/bin/godot"
            "${CMAKE_SOURCE_DIR}/tools/godot/bin/Godot.app/Contents/MacOS/Godot"
        )
    else()
        set(_dino_godot_candidates
            "${CMAKE_SOURCE_DIR}/tools/godot/bin/godot"
            "${CMAKE_SOURCE_DIR}/tools/godot/bin/Godot_v4.6-stable_linux.x86_64"
            "${CMAKE_SOURCE_DIR}/tools/godot/bin/Godot_v4.6-stable_linux.arm64"
        )
    endif()
    foreach(_c IN LISTS _dino_godot_candidates)
        if(EXISTS "${_c}")
            set(GODOT_EXECUTABLE "${_c}" CACHE FILEPATH "Godot 4.6 editor" FORCE)
            break()
        endif()
    endforeach()
endif()

if(DINO_GODOT)
    include(FetchContent)
    FetchContent_Declare(
        godot-cpp
        GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
        GIT_TAG ${DINO_GODOT_CPP_TAG}
    )
    FetchContent_MakeAvailable(godot-cpp)
    add_library(dino_godot SHARED src/godot/register_types.cpp src/godot/dino_island_node.cpp)
    target_include_directories(dino_godot PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/godot)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_libraries(dino_godot PRIVATE dino godot-cpp.template_debug)
    else()
        target_link_libraries(dino_godot PRIVATE dino godot-cpp.template_release)
    endif()
    set_target_properties(dino_godot PROPERTIES
        OUTPUT_NAME dino_godot
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(dino_godot PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_BINARY_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_BINARY_DIR}"
            ARCHIVE_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_BINARY_DIR}"
        )
    endforeach()
    if(WIN32)
        set_target_properties(dino_godot PROPERTIES PREFIX "")
    endif()
    add_custom_command(TARGET dino_godot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/godot/bin
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:dino_godot> ${CMAKE_SOURCE_DIR}/godot/bin/
    )
    if(GODOT_EXECUTABLE)
        add_custom_target(godot_editor
            COMMAND "${GODOT_EXECUTABLE}" --editor --path "${CMAKE_SOURCE_DIR}/godot"
            DEPENDS dino_godot
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            USES_TERMINAL
        )
        add_custom_target(godot_run
            COMMAND "${GODOT_EXECUTABLE}" --path "${CMAKE_SOURCE_DIR}/godot"
            DEPENDS dino_godot
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            USES_TERMINAL
        )
    endif()
endif()
