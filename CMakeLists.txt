cmake_minimum_required(VERSION 3.20)

project(PixelCoreSNES
    VERSION 0.1.0
    DESCRIPTION "A Super Nintendo Entertainment System emulator for Linux"
    LANGUAGES CXX
)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

add_library(pixelcore-snes-core
    core/memory/snes_memory.cpp
    core/memory/snes_memory.h
    core/cpu/w65816.cpp
    core/cpu/w65816.h
)

target_include_directories(pixelcore-snes-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(pixelcore-snes-core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

qt_add_executable(pixelcore-snes
    app/main.cpp
)

target_link_libraries(pixelcore-snes PRIVATE Qt6::Widgets pixelcore-snes-core)
target_compile_definitions(pixelcore-snes PRIVATE
    PIXELCORE_SNES_VERSION="${PROJECT_VERSION}"
)

install(TARGETS pixelcore-snes
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

include(CTest)

if(BUILD_TESTING)
    add_executable(snes-memory-tests tests/snes_memory_tests.cpp)
    target_link_libraries(snes-memory-tests PRIVATE pixelcore-snes-core)
    target_compile_options(snes-memory-tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    )
    add_test(NAME snes-memory-tests COMMAND snes-memory-tests)

    add_executable(w65816-tests tests/w65816_tests.cpp)
    target_link_libraries(w65816-tests PRIVATE pixelcore-snes-core)
    target_compile_options(w65816-tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    )
    add_test(NAME w65816-cpu-tests COMMAND w65816-tests)
endif()
