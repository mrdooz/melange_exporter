cmake_minimum_required(VERSION 2.6)
project(melange_exporter)

file(GLOB SRC "*.cpp" "*.hpp")
add_executable(${PROJECT_NAME} ${SRC})

macro(FIND_AND_ADD_FRAMEWORK fwname appname)
    find_library(FRAMEWORK_${fwname}
        NAMES ${fwname}
        PATHS ${CMAKE_OSX_SYSROOT}/System/Library
        PATH_SUFFIXES Frameworks
        NO_DEFAULT_PATH)
    if( ${FRAMEWORK_${fwname}} STREQUAL FRAMEWORK_${fwname}-NOTFOUND)
        MESSAGE(ERROR ": Framework ${fwname} not found")
    else()
        TARGET_LINK_LIBRARIES(${appname} ${FRAMEWORK_${fwname}})
        # MESSAGE(STATUS "Framework ${fwname} found at ${FRAMEWORK_${fwname}}")
    endif()
endmacro(FIND_AND_ADD_FRAMEWORK)

if (APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    include_directories("/Users/dooz/projects/melange15.0_131581/includes" )

    FIND_AND_ADD_FRAMEWORK(CoreAudio ${PROJECT_NAME})
    FIND_AND_ADD_FRAMEWORK(CoreFoundation ${PROJECT_NAME})
    FIND_AND_ADD_FRAMEWORK(AppKit ${PROJECT_NAME})
    FIND_AND_ADD_FRAMEWORK(IOKit ${PROJECT_NAME})
    FIND_AND_ADD_FRAMEWORK(Carbon ${PROJECT_NAME})

    target_link_libraries(${PROJECT_NAME}
        "/Users/dooz/projects/melange15.0_131581/libraries/osx/debug/libmelangelib.a"
        "/Users/dooz/projects/melange15.0_131581/libraries/osx/debug/libjpeglib.a"
    )

else()
endif()

