# runtime/CMakeLists.txt
# GameBoy Runtime Library

add_library(gbrt STATIC
    src/gbrt.c
)

target_include_directories(gbrt PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gbrt PUBLIC c_std_11)

# Platform-specific settings
if(APPLE)
    target_compile_definitions(gbrt PRIVATE GB_PLATFORM_MACOS)
elseif(WIN32)
    target_compile_definitions(gbrt PRIVATE GB_PLATFORM_WINDOWS)
else()
    target_compile_definitions(gbrt PRIVATE GB_PLATFORM_LINUX)
endif()

# SDL2 for platform layer (optional)
find_package(SDL2 QUIET)
if(SDL2_FOUND)
    target_link_libraries(gbrt PRIVATE SDL2::SDL2)
    target_compile_definitions(gbrt PRIVATE GB_HAS_SDL2)
endif()
