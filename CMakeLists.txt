cmake_minimum_required(VERSION 3.22)

# ============================================================
# PA System - Qt 6.11.1 + MSVC 2022 x64
# This CMake file coexists with PA.vcxproj.
# VS uses the .vcxproj; VSCode uses this CMakeLists.txt.
# Both builds are independent and won't interfere.
# ============================================================

project(PA VERSION 1.0 LANGUAGES CXX)

# 閳光偓閳光偓 C++ standard 閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 閳光偓閳光偓 Qt 6 configuration 閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓
# Point to your Qt installation. Adjust if your path differs.
set(QT_VERSION 6.11.1)
set(CMAKE_PREFIX_PATH "D:/Qt/${QT_VERSION}/msvc2022_64")

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Network
    Widgets
)

qt_standard_project_setup()

# 閳光偓閳光偓 Source files 閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓
set(SOURCES
    main.cpp
    Logging/Logger.cpp

    # MainWindow
    MainWindow/MainWindow.cpp
    MainWindow/HeaderBar.cpp

    # Pages
    Pages/HomePage/HomePage.cpp
    Pages/ParamPage/ParamPage.cpp
    Pages/ParamPage/TransmitParamPage.cpp
    Pages/ParamPage/ReceiveParamPage.cpp
    Pages/ParamPage/GateParamPage.cpp
    Pages/ParamPage/ProbeParamPage.cpp
    Pages/ParamPage/WedgeParamPage.cpp
    Pages/ParamPage/WorkpieceParamPage.cpp
    Pages/ParamPage/ScanParamPage.cpp
    Pages/ParamPage/TcgParamPage.cpp
    Pages/ParamPage/ImagingParamPage.cpp
    Pages/ParamPage/EncoderParamPage.cpp
    Pages/ParamPage/AnalysisParamPage.cpp
    Pages/ParamPage/ParamPage.TransmitPage.cpp
    Pages/ParamPage/ParamPage.ReceivePage.cpp
    Pages/ParamPage/ParamPage.GatePage.cpp
    Pages/ParamPage/ParamPage.ProbePage.cpp
    Pages/ParamPage/ParamPage.WedgePage.cpp
    Pages/ParamPage/ParamPage.WorkpiecePage.cpp
    Pages/ParamPage/ParamPage.ScanPage.cpp
    Pages/ParamPage/ParamPage.CalibrationPage.cpp
    Pages/ParamPage/ParamPage.CScanPages.cpp
    Pages/ParamPage/ParamPage.Model.cpp
    Pages/MeasurePage/MeasurePage.cpp

    # Ultrasonic
    Ultrasonic/CalibrationController.cpp
    Ultrasonic/DataPacketProcessor.cpp
    Ultrasonic/ConnectionManager.cpp
    Ultrasonic/ParameterDispatcher.cpp
    Ultrasonic/CScanIOManager.cpp
    Ultrasonic/CTSPA22SDriver.cpp
    Ultrasonic/CScanDataCodec.cpp
    Ultrasonic/CScanEngine.cpp

    # Widgets
    Widgets/AScanWidget/AScanWidget.cpp
    Widgets/BScanWidget/BScanWidget.cpp
    Widgets/CScanWidget/CScanWidget.cpp
)

set(HEADERS
    Logging/Logger.h
    # MainWindow
    MainWindow/MainWindow.h
    MainWindow/HeaderBar.h

    # Pages
    Pages/HomePage/HomePage.h
    Pages/ParamPage/ParamPage.h
    Pages/ParamPage/TransmitParamPage.h
    Pages/ParamPage/ReceiveParamPage.h
    Pages/ParamPage/GateParamPage.h
    Pages/ParamPage/ProbeParamPage.h
    Pages/ParamPage/WedgeParamPage.h
    Pages/ParamPage/WorkpieceParamPage.h
    Pages/ParamPage/ScanParamPage.h
    Pages/ParamPage/TcgParamPage.h
    Pages/ParamPage/ImagingParamPage.h
    Pages/ParamPage/EncoderParamPage.h
    Pages/ParamPage/AnalysisParamPage.h
    Pages/ParamPage/ParamPageUiHelpers.h
    Pages/MeasurePage/MeasurePage.h

    # Ultrasonic
    Ultrasonic/CalibrationController.h
    Ultrasonic/DataPacketProcessor.h
    Ultrasonic/ConnectionManager.h
    Ultrasonic/ParameterDispatcher.h
    Ultrasonic/CScanIOManager.h
    Ultrasonic/CTSPA22SDriver.h
    Ultrasonic/ColorMap.h
    Ultrasonic/ReadingCalculator.h
    Ultrasonic/Theme.h
    Ultrasonic/CScanDataCodec.h
    Ultrasonic/CScanEngine.h
    Ultrasonic/DataTypes.h
    Ultrasonic/IDriver.h
    Ultrasonic/PAParams.h

    # Widgets
    Widgets/AScanWidget/AScanWidget.h
    Widgets/BScanWidget/BScanWidget.h
    Widgets/CScanWidget/CScanWidget.h
)

set(RESOURCES
    resources/app_resources.qrc
)

if(WIN32)
    list(APPEND RESOURCES resources/app.rc)
endif()

# 閳光偓閳光偓 Build executable 閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓
qt_add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

# VS 解决方案资源管理器筛选器：按目录树组织源文件
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX "Source Files"
    FILES ${SOURCES}
)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX "Header Files"
    FILES ${HEADERS}
)

# 閳光偓閳光偓 Explicitly link Qt targets 閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓
# CMake 4.x with VS generator may not propagate Qt interface
# libraries properly. Explicit target_link_libraries ensures
# both include dirs and import libs are resolved.
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
)

# 閳光偓閳光偓 Include directories 閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/MainWindow
    ${CMAKE_CURRENT_SOURCE_DIR}/Pages/HomePage
    ${CMAKE_CURRENT_SOURCE_DIR}/Pages/ParamPage
    ${CMAKE_CURRENT_SOURCE_DIR}/Pages/MeasurePage
    ${CMAKE_CURRENT_SOURCE_DIR}/Ultrasonic
    ${CMAKE_CURRENT_SOURCE_DIR}/Widgets/AScanWidget
    ${CMAKE_CURRENT_SOURCE_DIR}/Widgets/BScanWidget
    ${CMAKE_CURRENT_SOURCE_DIR}/Widgets/CScanWidget
)

# 閳光偓閳光偓 MSVC-specific settings 閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /MP                          # Multi-processor compilation
        /W3                          # Warning level 3
        /utf-8                       # UTF-8 source encoding
        /wd4996                      # Suppress "deprecated" warnings
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        /SUBSYSTEM:WINDOWS
    )
endif()

# 閳光偓閳光偓 Qt-specific settings 閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓
set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

# Copy Qt DLLs and platform plugin next to the built executable
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::Core>"
            "$<TARGET_FILE:Qt6::Gui>"
            "$<TARGET_FILE:Qt6::Network>"
            "$<TARGET_FILE:Qt6::Widgets>"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_PREFIX_PATH}/plugins/platforms/qwindows$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms/"
    )
endif()
