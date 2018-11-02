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
    usbdevicelistmodel.cpp
    upgradeThread.cpp
    ./protodef/cpp/messages.pb.cc
)
add_executable(update_tools WIN32 ${update_tools_SRCS})
target_link_libraries(update_tools Qt5::Widgets)

target_compile_options (update_tools PUBLIC ${LIBUSB_CFLAGS} )
TARGET_LINK_LIBRARIES(update_tools ${Protobuf_LIBRARIES})
TARGET_LINK_LIBRARIES (update_tools debug ${LIBUSB_LIBRARIES} ${LIBUSB_LIBRARIES_DEBUG})
TARGET_LINK_LIBRARIES(update_tools optimized  ${LIBUSB_LIBRARIES} ${LIBUSB_LIBRARIES_REL})
