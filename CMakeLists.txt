# ----------------------------------------------------------------------------
# 
# ----------------------------------------------------------------------------
cmake_minimum_required(VERSION 2.8)
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}) 
set(CMAKE_C_FLAGS "/EHsc")
set(CMAKE_CXX_FLAGS "/EHsc")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")

add_subdirectory(glfw)

set(project_name viewer)

project(${project_name})

#add_definitions(-DIMGUI_IMPL_OPENGL_LOADER_GL3W)
set (CMAKE_CXX_STANDARD 17)

if(WIN32)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CMAKE_LIBRARY_ARCHITECTURE x64)
  else()
    set(CMAKE_LIBRARY_ARCHITECTURE x86)
  endif()
endif()


#----------------------------------------------------------------------------  
#  
# ----------------------------------------------------------------------------
SET(MAIN_SOURCE
			   main/main.cpp
			   glew/src/glew.c
)


SET(IMGUI_SOURCE
			   imgui/backends/imgui_impl_glfw.cpp
			   imgui/backends/imgui_impl_opengl3.cpp
			   imgui/imgui.cpp
			   imgui/imgui_draw.cpp
			   imgui/imgui_demo.cpp
			   imgui/imgui_tables.cpp
			   imgui/imgui_widgets.cpp
)

SET(SOURCE ${MAIN_SOURCE} ${IMGUI_SOURCE})

#----------------------------------------------------------------------------  
#  
# ----------------------------------------------------------------------------
SET(MAIN_HEADERS
			   
)

SET(IMGUI_HEADERS
			  
)

#----------------------------------------------------------------------------  
#  
# ----------------------------------------------------------------------------
file(GLOB ADD_HEADERS "./imgui/*.h*")
SET(IMGUI_HEADERS ${IMGUI_HEADERS} ${ADD_HEADERS})
SET(HEADERS ${MAIN_HEADERS} ${NATIVE_DIALOG_HEADERS} ${IMGUI_HEADERS})


# SOURCE_GROUP("Source Files" FILES ${SOURCE})
SOURCE_GROUP("Source Files/main" FILES ${MAIN_SOURCE})
SOURCE_GROUP("Source Files/imgui" FILES ${IMGUI_SOURCE})

SOURCE_GROUP("Header Files/main" FILES ${MAIN_HEADERS})
SOURCE_GROUP("Header Files/imgui" FILES ${IMGUI_HEADERS})
# ----------------------------------------------------------------------------   
# 
# ----------------------------------------------------------------------------
INCLUDE_DIRECTORIES("./")
include_directories(stb)
INCLUDE_DIRECTORIES("glew/include")
INCLUDE_DIRECTORIES("imgui")
INCLUDE_DIRECTORIES("imgui/examples")
INCLUDE_DIRECTORIES("imgui/backends")
INCLUDE_DIRECTORIES("imgui/examples/libs/glfw/include")
INCLUDE_DIRECTORIES("imgui_plot/include")
INCLUDE_DIRECTORIES("main")
# ----------------------------------------------------------------------------  
# 
# ----------------------------------------------------------------------------
  ADD_EXECUTABLE( ${project_name}
  				  ${SOURCE}
  				  ${HEADERS}
  				  )
# ----------------------------------------------------------------------------
# 
# ----------------------------------------------------------------------------
TARGET_LINK_LIBRARIES(${project_name}
					  optimized ${CMAKE_BINARY_DIR}/Release/glfw3.lib
					  debug 	${CMAKE_BINARY_DIR}/Debug/glfw3.lib				  
					  opengl32.lib
					  )

# ----------------------------------------------------------------------------
# 
# ----------------------------------------------------------------------------
set_target_properties( ${project_name} PROPERTIES  VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${project_name})

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/fonts DESTINATION ${CMAKE_BINARY_DIR}/Release)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/fonts DESTINATION ${CMAKE_BINARY_DIR}/Debug)
 

