cmake_minimum_required(VERSION 3.16)

project(OscilloscopePlayer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Путь к FFmpeg 
set(FFMPEG_ROOT "L:/custom_ffmpeg")

# Указание путей к инклудерам
set(AVFORMAT_INCLUDE_DIR "${FFMPEG_ROOT}/include")
set(AVCODEC_INCLUDE_DIR "${FFMPEG_ROOT}/include")
set(AVUTIL_INCLUDE_DIR "${FFMPEG_ROOT}/include")
set(SWSCALE_INCLUDE_DIR "${FFMPEG_ROOT}/include")
set(SWRESAMPLE_INCLUDE_DIR "${FFMPEG_ROOT}/include")

# Указание библиотек
set(AVFORMAT_LIBRARY "${FFMPEG_ROOT}/lib/libavformat.a")
set(AVCODEC_LIBRARY "${FFMPEG_ROOT}/lib/libavcodec.a")
set(AVUTIL_LIBRARY "${FFMPEG_ROOT}/lib/libavutil.a")
set(SWSCALE_LIBRARY "${FFMPEG_ROOT}/lib/libswscale.a")
set(SWRESAMPLE_LIBRARY "${FFMPEG_ROOT}/lib/libswresample.a")

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Multimedia)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Multimedia)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    videoviewer.cpp
    videoviewer.h
    decode.cpp
    decode.h
    oscilloscope.cpp
    oscilloscope.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(${PROJECT_NAME}
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(${PROJECT_NAME} SHARED ${PROJECT_SOURCES})
    else()
        add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
    endif()
endif()

# Подключение инклудов FFmpeg
target_include_directories(${PROJECT_NAME} PRIVATE
    ${AVFORMAT_INCLUDE_DIR}
    ${AVCODEC_INCLUDE_DIR}
    ${AVUTIL_INCLUDE_DIR}
    ${SWSCALE_INCLUDE_DIR}
    ${SWRESAMPLE_INCLUDE_DIR}
)

# Линковка с Qt и FFmpeg
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    ${AVFORMAT_LIBRARY}
    ${AVCODEC_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${SWSCALE_LIBRARY}
    ${SWRESAMPLE_LIBRARY}
    ws2_32
    bcrypt
)

# Свойства сборки
set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER ${PROJECT_NAME}.yjbeetle.art
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Установка
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()

# Qt deploy (Windows/macOS/Android)
if(NOT CMAKE_BUILD_TYPE MATCHES Debug)
    get_target_property(_qmake_executable Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)

    if(WIN32)
        find_program(DEPLOYQT_EXECUTABLE NAMES windeployqt HINTS "${_qt_bin_dir}")
        add_custom_command(TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${CMAKE_BINARY_DIR}/${PROJECT_NAME}_output/"
            COMMAND ${DEPLOYQT_EXECUTABLE}
                --dir "${CMAKE_BINARY_DIR}/${PROJECT_NAME}_output/"
                "$<TARGET_FILE:${PROJECT_NAME}>"
                --$<LOWER_CASE:$<CONFIG>>
                --qmldir ${CMAKE_SOURCE_DIR}
        )
    endif()
endif()
