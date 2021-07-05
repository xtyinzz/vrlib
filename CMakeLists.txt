cmake_minimum_required(VERSION 3.10)

# set the project name
project(vrlib)

# source files for vrlib
file(GLOB_RECURSE SOURCES 
	${CMAKE_SOURCE_DIR}/src/*.c
	${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/vr/*.cpp
    ${CMAKE_SOURCE_DIR}/src/vr/*.c
    ${CMAKE_SOURCE_DIR}/src/vr/*.C
    )
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/vr/testmain.C") # remove testmain.C which contains another main method for legacy vrlib testing

# source files for legacy vrlib testmain
file(GLOB_RECURSE VR_TESTMAIN_SOURCES 
    ${CMAKE_SOURCE_DIR}/src/vr/*.cpp
    ${CMAKE_SOURCE_DIR}/src/vr/*.c
    ${CMAKE_SOURCE_DIR}/src/vr/*.C
    )

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

# store root directory for program use of absolute path
configure_file(configuration/root_directory.h.in configuration/root_directory.h) 
include_directories(${CMAKE_BINARY_DIR}/configuration)

# add compiler definitions for vr
add_compile_definitions(REAL=float)

# add library
# find the required packages
find_package(GLM REQUIRED)
message(STATUS "GLM included at ${GLM_INCLUDE_DIR}")
find_package(GLFW3 REQUIRED)
message(STATUS "Found GLFW3 in ${GLFW3_INCLUDE_DIR}")

# first create relevant static libraries requried for other projects
add_library(STB_IMAGE "${PROJECT_SOURCE_DIR}/src/util/stb_image.cpp")
add_library(GLAD "${PROJECT_SOURCE_DIR}/src/util/glad.c")
set(LIBS GLAD ${GLFW3_LIBRARY} STB_IMAGE)

# generate compile commands for vs code
add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)
# make executables to bin folder
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
# add the executable
add_executable(${PROJECT_NAME} ${SOURCES})
add_executable(testmain ${VR_TESTMAIN_SOURCES})

# Set the directories that should be included in the build command for this target
# when running g++ these will be included as -I/directory/path/
include_directories(${PROJECT_SOURCE_DIR}/includes)

target_link_libraries(${PROJECT_NAME}
                      ${CMAKE_DL_LIBS}
                      ${LIBS})


# copy required files for the executable
# copy each shader script to the executable folder
file(GLOB SHADER_SOURCE
            "src/shader/*.vert"
            "src/shader/*.frag"
            "src/shader/*.geom"
)
set(SHADER_DESTINATION "${PROJECT_SOURCE_DIR}/bin/shader")
file(MAKE_DIRECTORY ${SHADER_DESTINATION})
foreach(SHADER ${SHADER_SOURCE})
    file(COPY ${SHADER} DESTINATION ${SHADER_DESTINATION})
endforeach(SHADER)