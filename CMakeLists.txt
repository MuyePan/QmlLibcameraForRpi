cmake_minimum_required(VERSION 3.16)

project(QmlLibcamera VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.2 COMPONENTS Quick REQUIRED)

qt_add_executable(appQmlLibcamera
    qlibcamera/common/event_loop.cpp
    qlibcamera/common/event_loop.h
    qlibcamera/common/image.cpp
    qlibcamera/common/image.h
    qlibcamera/common/options.cpp
    qlibcamera/common/options.h
    qlibcamera/common/ppm_writer.cpp
    qlibcamera/common/ppm_writer.h
    qlibcamera/common/stream_options.cpp
    qlibcamera/common/stream_options.h

    qlibcamera/format_converter.cpp
    qlibcamera/format_converter.h
    qlibcamera/format_converter_yuv.cpp
    qlibcamera/format_converter_yuv.h
    qlibcamera/qlibcameraview.h
    qlibcamera/qlibcameraview.cpp
    qlibcamera/qlibcamera.h
    qlibcamera/qlibcamera.cpp
    qlibcamera/qlibcameraworker.h
    qlibcamera/qlibcameraworker.cpp

    main.cpp
)

qt_add_qml_module(appQmlLibcamera
    URI QmlLibcamera
    VERSION 1.0
    QML_FILES main.qml 
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appQmlLibcamera PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appQmlLibcamera
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

find_package(PkgConfig)
pkg_check_modules(LIBCAMERA REQUIRED IMPORTED_TARGET libcamera)
pkg_check_modules(LIBEVENT REQUIRED IMPORTED_TARGET libevent)
pkg_check_modules(LIBEVENT_THREAD REQUIRED IMPORTED_TARGET libevent_pthreads)
pkg_check_modules(LIBAVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(LIBAVUTIL REQUIRED IMPORTED_TARGET libavutil)

target_compile_definitions(appQmlLibcamera PRIVATE QT_NO_KEYWORDS)

target_link_libraries(appQmlLibcamera
    PRIVATE Qt6::Quick PkgConfig::LIBCAMERA PkgConfig::LIBEVENT PkgConfig::LIBEVENT_THREAD PkgConfig::LIBAVCODEC  PkgConfig::LIBAVUTIL)

include_directories(qlibcamera/)
include(GNUInstallDirs)
install(TARGETS appQmlLibcamera
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
