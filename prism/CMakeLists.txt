
cmake_minimum_required(VERSION 3.5)
project(prism)
include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)

# Set build options
option(PRISM_STANDALONE "Build as a PRISM_STANDALONE executable" ON)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(DEBUG_PARSE "Enable debug node" OFF)

################################################################################
# Set target arch type if empty. Visual studio solution generator provides it.
################################################################################
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(NOT CMAKE_VS_PLATFORM_NAME)
        set(CMAKE_VS_PLATFORM_NAME "x64")
    endif()
    message("${CMAKE_VS_PLATFORM_NAME} architecture in use")

    if(NOT ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64"
        OR "${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32"
	OR "${CMAKE_VS_PLATFORM_NAME}" STREQUAL "ARM64"))
        message(FATAL_ERROR "${CMAKE_VS_PLATFORM_NAME} arch is not supported!")
    endif()
endif()

# Source files

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
file(GLOB_RECURSE CXX_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/lib/strhash64/*.cpp)
file(GLOB_RECURSE C_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c ${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.c ${CMAKE_CURRENT_SOURCE_DIR}/lib/**/*.c)
set(SRC_DIR ${CXX_FILES} ${C_FILES})

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

# Build
if (PRISM_STANDALONE)
    add_definitions(-DPRISM_STANDALONE)
    add_executable(${PROJECT_NAME} ${SRC_DIR})
	set(LINK_TYPE "MD")
else()
    add_library(${PROJECT_NAME} STATIC ${SRC_DIR})
	set(LINK_TYPE "MT")
endif()

if (DEBUG_PARSE)
    add_definitions(-DDEBUG_PARSE)
endif()

if(NOT MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "-g")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
    set(CMAKE_CXX_FLAGS "-Wno-narrowing")
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:
            /w;
            /Od;
            /ZI;
            /${LINK_TYPE}d
        >
        $<$<CONFIG:Release>:
            /O3;
            /Gy;
            /W3;
            /${LINK_TYPE}
        >
        /permissive-;
        /MP;
        ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
        ${DEFAULT_CXX_EXCEPTION_HANDLING}
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:
            /INCREMENTAL
        >
        $<$<CONFIG:Release>:
            /OPT:REF;
            /OPT:ICF;
            /INCREMENTAL:NO;
            /FORCE:MULTIPLE
        >
        /MANIFEST:NO;
        /DEBUG;
        /SUBSYSTEM:CONSOLE
    )
    if (NOT "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
        target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
        target_link_options(${PROJECT_NAME} PRIVATE /bigobj)
    endif()

    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Fetch Dependencies

if(NOT PRISM_STANDALONE AND EXISTS "/mnt/c/WINDOWS/system32/wsl.exe")
    FetchContent_Declare(
        GSL
        GIT_REPOSITORY https://github.com/Microsoft/GSL.git
        GIT_TAG a3534567187d2edc428efd3f13466ff75fe5805c
    )
    FetchContent_MakeAvailable(GSL)
    target_link_libraries(${PROJECT_NAME} PRIVATE GSL)
endif()

if(PRISM_STANDALONE)
	FetchContent_Declare(
		spdlog
		GIT_REPOSITORY https://github.com/gabime/spdlog.git
		GIT_TAG 7e635fca68d014934b4af8a1cf874f63989352b7
	)

	FetchContent_MakeAvailable(spdlog)
	target_link_libraries(${PROJECT_NAME} PRIVATE spdlog)
else()
	find_package(spdlog QUIET)
    target_link_libraries(${PROJECT_NAME} PUBLIC spdlog::spdlog)
endif()

if(NOT PRISM_STANDALONE)
    target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib)
    target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

if(PRISM_STANDALONE)
    enable_testing()
    add_test(NAME prism COMMAND prism ../examples/script.opengl.fs)
endif()
