cmake_minimum_required(VERSION 3.20)
project(CopyHeap VERSION 1.0.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable Unicode globally
add_compile_definitions(UNICODE _UNICODE)

# Ensure UTF-8 execution and source charset
if(MSVC)
    add_compile_options(/utf-8 /W4 /permissive-)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_compile_options(-Wall -Wextra -Wno-unknown-pragmas)
    set(STATIC_LINK_FLAGS "-static -static-libgcc -static-libstdc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${STATIC_LINK_FLAGS}")
endif()

include_directories(src res)

# Core source files (no GUI dependency for easy testing)
set(CORE_SOURCES
    src/core/models.cpp
    src/core/storage_manager.cpp
    src/core/fnv_hash.cpp
    src/core/string_utils.cpp
)

# App source files
set(APP_SOURCES
    src/main.cpp
    src/clipboard/clipboard_watcher.cpp
    src/tray/tray_icon_manager.cpp
    src/ui/main_window.cpp
    src/ui/gdi_renderer.cpp
    res/CopyHeap.rc
)

# Windows system libraries
set(SYSTEM_LIBS
    kernel32
    user32
    gdi32
    gdiplus
    dwmapi
    shell32
    comctl32
    ole32
    uuid
    shlwapi
)

# Main executable
add_executable(CopyHeap WIN32 ${CORE_SOURCES} ${APP_SOURCES})
target_link_libraries(CopyHeap PRIVATE ${SYSTEM_LIBS})

if(MINGW)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/empty_manifest)
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} -c -x c NUL -o ${CMAKE_BINARY_DIR}/empty_manifest/default-manifest.o
    )
    target_link_options(CopyHeap PRIVATE -B${CMAKE_BINARY_DIR}/empty_manifest -mwindows -municode)
endif()

# Test executable
enable_testing()

set(TEST_SOURCES
    tests/test_main.cpp
    tests/test_storage.cpp
    tests/test_limits.cpp
    tests/test_search.cpp
    tests/test_ui_text.cpp
    src/ui/main_window.cpp
    src/ui/gdi_renderer.cpp
    src/tray/tray_icon_manager.cpp
    src/clipboard/clipboard_watcher.cpp
)

add_executable(CopyHeap_Tests ${CORE_SOURCES} ${TEST_SOURCES})
target_link_libraries(CopyHeap_Tests PRIVATE ${SYSTEM_LIBS})

if(MINGW)
    target_link_options(CopyHeap_Tests PRIVATE -mconsole -municode)
endif()

add_test(NAME AllTests COMMAND CopyHeap_Tests)
