# CMakeLists.txt for tests/test-waterfall

# Specify the project name
SET(targetname test-waterfall)

# Specify the project files
SET(sources
main.cpp
)

# Specify the source groups
SOURCE_GROUP(.cpp FILES ${sources})

# Specify the necessary paths
INCLUDE_DIRECTORIES(
${millipede_SOURCE_DIR}/..
${millipede_SOURCE_DIR}/../libraries/boost_1_39_0
)

# Specify the target and where to put it
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${millipede_BINARY_DIR}/bin/tests/${targetname}/bin)
ADD_EXECUTABLE(${targetname} ${sources})
IF(MSVC_IDE)
	# A hack to get around the "Debug" and "Release" directories Visual Studio tries to add
    SET_TARGET_PROPERTIES(${targetname} PROPERTIES PREFIX "../")
    SET_TARGET_PROPERTIES(${targetname} PROPERTIES IMPORT_PREFIX "../")
ENDIF(MSVC_IDE)

# Specify the necessary libraries to link
TARGET_LINK_LIBRARIES(${targetname} common)

# Specify things to install
INSTALL(TARGETS ${targetname} DESTINATION bin/tests/${targetname}/bin)