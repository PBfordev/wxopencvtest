###############################################################################
## Name:        CMakeLists.txt
## Purpose:     To build wxTestOpenCV application
## Author:      PB
## Created:     2020-09-16
## Copyright:   (c) 2020 PB
## Licence:     wxWindows licence
###############################################################################

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(wxTestOpenCV)

find_package(wxWidgets 3.1.0 COMPONENTS core base REQUIRED)
find_package(OpenCV 4.2 REQUIRED)

set(SOURCES
  convertmattowxbmp.h
  bmpfromocvpanel.h
  ocvframe.h
  convertmattowxbmp.cpp
  bmpfromocvpanel.cpp
  ocvframe.cpp
  ocvapp.cpp
)

if (WIN32)
  list(APPEND SOURCES "${wxWidgets_ROOT_DIR}/include/wx/msw/wx.rc")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property (DIRECTORY PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

add_executable(${PROJECT_NAME} ${SOURCES})

include(${wxWidgets_USE_FILE})

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)

if (WIN32)
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    wxUSE_RC_MANIFEST
    wxUSE_DPI_AWARE_MANIFEST=2)
  set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE YES)

  if (MSVC)
     set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
  endif(MSVC)

elseif(APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE YES)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${wxWidgets_LIBRARIES} ${OpenCV_LIBS})