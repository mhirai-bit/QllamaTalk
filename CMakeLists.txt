cmake_minimum_required(VERSION 3.21.1)

option(LINK_INSIGHT "Link Qt Insight Tracker library" ON)
option(BUILD_QDS_COMPONENTS "Build design studio components" ON)

project(QllamaTalkApp LANGUAGES CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/llama_setup.cmake)
include_directories(${LLAMA_INCLUDE_DIR})
link_directories(${LLAMA_LIB_DIR})

set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Qml Quick)

if (Qt6_VERSION VERSION_GREATER_EQUAL 6.3)
    qt_standard_project_setup()
endif()

qt_add_executable(QllamaTalkApp src/main.cpp)
target_include_directories(QllamaTalkApp PRIVATE "/usr/local/include")

qt_add_resources(QllamaTalkApp "configuration"
    PREFIX "/"
    FILES
        qtquickcontrols2.conf
        3rdparty/llama.cpp/models/Llama-3.1-8B-Open-SFT.Q4_K_M.gguf
)

target_link_libraries(QllamaTalkApp PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

if (BUILD_QDS_COMPONENTS)
    include(${CMAKE_CURRENT_SOURCE_DIR}/qmlcomponents)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/qmlmodules)

if (LINK_INSIGHT)
    include(${CMAKE_CURRENT_SOURCE_DIR}/insight)
endif ()

include(GNUInstallDirs)
install(TARGETS QllamaTalkApp
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# make IDEs aware of the QML import path
set(QML_IMPORT_PATH ${PROJECT_BINARY_DIR}/qml CACHE PATH
    "Path to the custom QML components defined by the project")
