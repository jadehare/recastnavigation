# Find dependencies
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(OpenGL REQUIRED)

# SDL2 setup - platform specific
if(WIN32)
	if(NOT SDL2_ROOT_DIR)
		set(SDL2_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Contrib/SDL")
	endif()
	find_package(SDL2 REQUIRED)
elseif(APPLE)
	if(NOT SDL2_ROOT_DIR)
		set(SDL2_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Bin")
	endif()
	set(SDL2_FRAMEWORK_PATH "${SDL2_ROOT_DIR}/SDL2.framework")
	if(NOT EXISTS "${SDL2_FRAMEWORK_PATH}/SDL2")
		message(FATAL_ERROR "SDL2.framework not found in ${SDL2_ROOT_DIR}; set SDL2_ROOT_DIR to its parent directory")
	endif()
	# Use the framework's Headers directory for <SDL.h> style includes
	set(SDL2_INCLUDE_DIR "${SDL2_FRAMEWORK_PATH}/Headers")
	# Also add the parent so <SDL2/SDL.h> style includes work (used internally by SDL headers)
	set(SDL2_INCLUDE_DIR_PARENT "${SDL2_ROOT_DIR}")
else()
	# Linux - use system SDL2
	find_package(SDL2 REQUIRED)
endif()

# Build the third-party Contrib library (must come after SDL2 setup since it uses SDL2 variables)
add_subdirectory(Contrib)

# RecastDemo executable
if(WIN32)
	add_executable(RecastDemo WIN32)
elseif(APPLE)
	add_executable(RecastDemo MACOSX_BUNDLE)
else()
	add_executable(RecastDemo)
endif()

target_sources(RecastDemo PRIVATE
	Source/AppPaths.cpp
	Source/AppState.cpp
	Source/InputGeom.cpp
	Source/main.cpp
	Source/PartitionedMesh.cpp
	Source/PerfTimer.cpp
	Source/Sample.cpp
	Source/Sample_SoloMesh.cpp
	Source/Sample_TempObstacles.cpp
	Source/Sample_TileMesh.cpp
	Source/SampleInterfaces.cpp
	Source/TestCase.cpp
	Source/Tool_ConvexVolume.cpp
	Source/Tool_Crowd.cpp
	Source/Tool_NavMeshPrune.cpp
	Source/Tool_NavMeshTester.cpp
	Source/Tool_OffMeshConnection.cpp
	Source/ValueHistory.cpp
)

target_include_directories(RecastDemo PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/Include
)

set_target_properties(RecastDemo PROPERTIES
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
)

# Platform-specific compiler options
if(APPLE)
	# Suppress deprecated OpenGL warnings (gluPerspective, gluProject, etc.)
	target_compile_options(RecastDemo PRIVATE -Wno-deprecated-declarations)
endif()

# Link libraries
target_link_libraries(RecastDemo PRIVATE
	OpenGL::GL
	OpenGL::GLU
	Contrib
	RecastNavigation::DebugUtils
	RecastNavigation::Detour
	RecastNavigation::DetourCrowd
	RecastNavigation::DetourTileCache
	RecastNavigation::Recast
)

# SDL2 linking - platform specific
if(APPLE)
	target_link_libraries(RecastDemo PRIVATE "${SDL2_FRAMEWORK_PATH}")
	target_include_directories(RecastDemo PRIVATE ${SDL2_INCLUDE_DIR} ${SDL2_INCLUDE_DIR_PARENT})
	target_compile_options(RecastDemo PRIVATE "-F${SDL2_ROOT_DIR}")
	set_target_properties(RecastDemo PROPERTIES
		MACOSX_BUNDLE_BUNDLE_NAME RecastDemo
		MACOSX_BUNDLE_GUI_IDENTIFIER org.recastnavigation.RecastDemo
		BUILD_WITH_INSTALL_RPATH TRUE
		INSTALL_RPATH "@executable_path/../Frameworks"
	)
else()
	target_link_libraries(RecastDemo PRIVATE SDL2::SDL2 SDL2::SDL2main)
endif()

# Copy runtime files to build directory
if(APPLE)
	# Font and test cases stay in the bundle. Models are copied beside the .app.
	file(GLOB_RECURSE demo_resources CONFIGURE_DEPENDS
		"${CMAKE_CURRENT_SOURCE_DIR}/Bin/TestCases/*"
	)
	list(APPEND demo_resources "${CMAKE_CURRENT_SOURCE_DIR}/Bin/DroidSans.ttf")
	foreach(resource IN LISTS demo_resources)
		file(RELATIVE_PATH resource_path "${CMAKE_CURRENT_SOURCE_DIR}/Bin" "${resource}")
		get_filename_component(resource_directory "${resource_path}" DIRECTORY)
		set_source_files_properties("${resource}" PROPERTIES
			MACOSX_PACKAGE_LOCATION "Resources/${resource_directory}"
			HEADER_FILE_ONLY TRUE
			EXTERNAL_OBJECT FALSE
		)
	endforeach()
	target_sources(RecastDemo PRIVATE ${demo_resources})
	# file(COPY) preserves the versioned framework's symlinks.
	# model, bin, and gset sit next to RecastDemo.app so they can be edited without opening the bundle.
	add_custom_command(TARGET RecastDemo POST_BUILD
		COMMAND ${CMAKE_COMMAND}
			"-DSDL2_FRAMEWORK_PATH=${SDL2_FRAMEWORK_PATH}"
			"-DBUNDLE_DIRECTORY=$<TARGET_BUNDLE_DIR:RecastDemo>"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopySDL2Framework.cmake"
		COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:RecastDemo>/../model"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Bin/Meshes" "$<TARGET_BUNDLE_DIR:RecastDemo>/../model"
		COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:RecastDemo>/../bin"
		COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:RecastDemo>/../gset"
		VERBATIM
	)
elseif(WIN32)
	add_custom_command(TARGET RecastDemo POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SDL2_RUNTIME_LIBRARY}" $<TARGET_FILE_DIR:RecastDemo>/
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Bin/Meshes $<TARGET_FILE_DIR:RecastDemo>/Meshes
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Bin/TestCases $<TARGET_FILE_DIR:RecastDemo>/TestCases
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/Bin/DroidSans.ttf $<TARGET_FILE_DIR:RecastDemo>/
	)
else()
	file(COPY Bin/Meshes DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
	file(COPY Bin/TestCases DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
	file(COPY Bin/DroidSans.ttf DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

# Installation
install(TARGETS RecastDemo
	RUNTIME DESTINATION bin
	BUNDLE DESTINATION bin
)
if(APPLE)
	install(DIRECTORY Bin/Meshes/ DESTINATION bin/model)
else()
	install(DIRECTORY Bin/Meshes DESTINATION bin)
	install(DIRECTORY Bin/TestCases DESTINATION bin)
	install(FILES Bin/DroidSans.ttf DESTINATION bin)
endif()

if(WIN32)
	install(FILES "${SDL2_RUNTIME_LIBRARY}" DESTINATION bin)
endif()

if(APPLE AND RECASTNAVIGATION_TESTS)
	# Exercise resource/save paths and geometry round trips without opening a window
	# or writing into the user's actual Application Support directory.
	add_executable(RecastDemoPathTests
		Tests/AppPathsTests.cpp
		Source/AppPaths.cpp
		Source/InputGeom.cpp
		Source/PartitionedMesh.cpp
		Source/PerfTimer.cpp
		Source/SampleInterfaces.cpp
	)
	set_target_properties(RecastDemoPathTests PROPERTIES
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
		BUILD_RPATH "${SDL2_ROOT_DIR}"
	)
	target_include_directories(RecastDemoPathTests PRIVATE Include ${SDL2_INCLUDE_DIR} ${SDL2_INCLUDE_DIR_PARENT})
	target_compile_options(RecastDemoPathTests PRIVATE "-F${SDL2_ROOT_DIR}" -Wno-deprecated-declarations)
	target_link_libraries(RecastDemoPathTests PRIVATE
		"${SDL2_FRAMEWORK_PATH}" OpenGL::GL RecastNavigation::DebugUtils
	)
	add_test(NAME RecastDemoPaths COMMAND RecastDemoPathTests)
endif()
