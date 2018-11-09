cmake_minimum_required(VERSION 3.10)

project(update_tools)

if (STATIC_BUILD)
SET( Protobuf_USE_STATIC_LIBS ON )
endif (STATIC_BUILD)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed
set(CMAKE_AUTOMOC ON)
# Create code from a list of Qt designer ui files
set(CMAKE_AUTOUIC ON)
set(CMAKE_MACOSX_BUNDLE TRUE)

SET(MACOSX_BUNDLE_INFO_STRING "keybox updtate tools")
SET(MACOSX_BUNDLE_ICON_FILE "icon")
SET(MACOSX_BUNDLE_GUI_IDENTIFIER keyboxd_update_tools)
SET(MACOSX_BUNDLE_LONG_VERSION_STRING "0.9.0")
SET(MACOSX_BUNDLE_BUNDLE_NAME "keybox update tools")
SET(MACOSX_BUNDLE_SHORT_VERSION_STRING "0.9.0")
SET(MACOSX_BUNDLE_BUNDLE_VERSION "0.9.0")
SET(MACOSX_BUNDLE_COPYRIGHT "MIT, by keybox team")

set( ICON_NAME "icon.icns" )
set( ICON_PATH "${PROJECT_SOURCE_DIR}/${ICON_NAME}" )

SET_SOURCE_FILES_PROPERTIES(${ICON_PATH} PROPERTIES
                             MACOSX_PACKAGE_LOCATION Resources)
SET_SOURCE_FILES_PROPERTIES("${PROJECT_SOURCE_DIR}/icon.ico" PROPERTIES VS_COPY_TO_OUT_DIR Always)


FIND_PACKAGE (Protobuf REQUIRED)

if (UNIX)
 find_package(PkgConfig)
 pkg_check_modules (LIBUSB REQUIRED libusb-1.0)
elseif (WIN32)
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -utf-8")
 LINK_LIBRARIES(ws2_32.lib)
 if (NOT IS_DIRECTORY $ENV{vcpkg_root})
  message(FATAL_ERROR "you must specify vcpkg_root for seeking packages")
 endif()
 message(STATUS "win32 selected. seeking libusb")
 if (NOT IS_DIRECTORY $ENV{vcpkg_root}/installed/x86-windows/include/libusb-1.0)
  message(FATAL_ERROR "please install libusb var vcpkg")
 endif()
 set(LIBUSB_CFLAGS -D_WIN32_WINNT=0x0601 -I$ENV{vcpkg_root}/installed/x86-windows/include/libusb-1.0)
 set(LIBUSB_LIBRARIES_DIR_DEBUG $ENV{vcpkg_root}/installed/x86-windows/debug)
 set(LIBUSB_LIBRARIES_DIR_REL $ENV{vcpkg_root}/installed/x86-windows)
 SET(LIBUSB_LIBRARIES_DEBUG ${LIBUSB_LIBRARIES_DIR_DEBUG}/lib/libusb-1.0.lib)
 SET(LIBUSB_LIBRARIES_REL ${LIBUSB_LIBRARIES_DIR_REL}/lib/libusb-1.0.lib)
endif()

INCLUDE_DIRECTORIES(./protodef)
INCLUDE_DIRECTORIES(./protodef/cpp)

# Find the QtWidgets library
find_package(Qt5Widgets CONFIG REQUIRED)

# Populate a CMake variable with the sources
set(update_tools_SRCS
    mainwindow.ui
    mainwindow.cpp
    main.cpp
    upgradeThread.cpp
    ./protodef/cpp/messages.pb.cc
    icon.icns
    icon.ico
	update_tools.rc
)
add_executable(update_tools WIN32 ${update_tools_SRCS})
target_link_libraries(update_tools Qt5::Widgets)

target_compile_options (update_tools PUBLIC ${LIBUSB_CFLAGS} )
TARGET_LINK_LIBRARIES(update_tools ${Protobuf_LIBRARIES})
TARGET_LINK_LIBRARIES (update_tools debug ${LIBUSB_LIBRARIES} ${LIBUSB_LIBRARIES_DEBUG})
TARGET_LINK_LIBRARIES(update_tools optimized  ${LIBUSB_LIBRARIES} ${LIBUSB_LIBRARIES_REL})
