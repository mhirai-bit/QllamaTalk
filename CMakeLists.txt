cmake_minimum_required(VERSION 3.21.1)

option(LINK_INSIGHT "Link Qt Insight Tracker library" ON)
option(BUILD_QDS_COMPONENTS "Build design studio components" ON)

project(QllamaTalkApp LANGUAGES CXX)

# ----------------------------------------------------------------------------
# ダウンロード & llama.cpp ビルド設定などを読み込む
#   (llama_setup.cmake の中で LLAMA_DYNAMIC_LIB_FILE_DIR 等が定義されている想定)
# ----------------------------------------------------------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/download_llama_model.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/llama_setup.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/llmRemoteServer_setup.cmake)
set(CMAKE_AUTOMOC ON)

# ----------------------------------------------------------------------------
# Qt モジュールの検索
# ----------------------------------------------------------------------------
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 TextToSpeech)

if (Qt6_VERSION VERSION_GREATER_EQUAL 6.3)
    qt_standard_project_setup()
endif()

# ----------------------------------------------------------------------------
# QllamaTalkApp 実行ファイルターゲットの定義
# ----------------------------------------------------------------------------
qt_add_executable(QllamaTalkApp
    src/main.cpp
)

if(ANDROID)
    target_sources(QllamaTalkApp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platforms/android/AndroidManifest.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/platforms/android/build.gradle
        ${CMAKE_CURRENT_SOURCE_DIR}/platforms/android/res/values/libs.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/platforms/android/res/xml/qtprovider_paths.xml
    )
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/download_whisper_model.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/whisper_setup.cmake)

if(IOS)
    set_target_properties(QllamaTalkApp
        PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/platforms/ios/Info.plist"
            QT_IOS_LAUNCH_SCREEN "${CMAKE_CURRENT_SOURCE_DIR}/platforms/ios/LaunchScreen.storyboard"
            XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
    )

    set(asset_catalog_path "platforms/ios/Assets.xcassets")
    target_sources(QllamaTalkApp PRIVATE "${asset_catalog_path}")
    set_source_files_properties(
        ${asset_catalog_path}
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources
    )
    qt_add_ios_ffmpeg_libraries(QllamaTalkApp)
endif()
if(ANDROID)
    set_property(TARGET QllamaTalkApp PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
                 "${CMAKE_CURRENT_SOURCE_DIR}/platforms/android")

endif()

# ----------------------------------------------------------------------------
# Qt Quick Controls の設定用 qrc
# ----------------------------------------------------------------------------
qt_add_resources(QllamaTalkApp "configuration"
    PREFIX "/"
    FILES
        qtquickcontrols2.conf
)

# ----------------------------------------------------------------------------
# QllamaTalkApp がリンクする Qt モジュール
# ----------------------------------------------------------------------------
target_link_libraries(QllamaTalkApp PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::TextToSpeech
)

# ----------------------------------------------------------------------------
# デザインスタジオ用のコンポーネントをビルド (オプション)
# ----------------------------------------------------------------------------
if (BUILD_QDS_COMPONENTS)
    include(${CMAKE_CURRENT_SOURCE_DIR}/qmlcomponents)
endif()

# ----------------------------------------------------------------------------
# QMLモジュールを検索パスへ登録
# ----------------------------------------------------------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/qmlmodules)

# ----------------------------------------------------------------------------
# Insight Tracker をリンク (オプション)
# ----------------------------------------------------------------------------
if (LINK_INSIGHT)
    include(${CMAKE_CURRENT_SOURCE_DIR}/insight)
endif()

# ----------------------------------------------------------------------------
# QllamaTalkApp のインストール先
# ----------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS QllamaTalkApp
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# make IDEs aware of the QML import path
set(QML_IMPORT_PATH ${PROJECT_BINARY_DIR}/qml CACHE PATH
    "Path to the custom QML components defined by the project")

# ----------------------------------------------------------------------------
# Android 用の "libllama.so" / "libggml.so" をインポートライブラリとして定義
# ----------------------------------------------------------------------------
#   llama_setup.cmake 等で LLAMA_DYNAMIC_LIB_FILE_DIR / GGML_DYNAMIC_LIB_FILE_DIR が設定済み
# ----------------------------------------------------------------------------
if(ANDROID)
    # 1) KDAB製の android_openssl を取り込む (FetchContent)
    include(FetchContent)
    FetchContent_Declare(
        android_openssl
        DOWNLOAD_EXTRACT_TIMESTAMP true
        URL https://github.com/KDAB/android_openssl/archive/refs/heads/master.zip
    )
    FetchContent_MakeAvailable(android_openssl)
    include(${android_openssl_SOURCE_DIR}/android_openssl.cmake)

    # 2) QllamaTalkApp に OpenSSL ライブラリを追加
    #    → これにより Qt が TLS backend として libssl/libcrypto を利用可能になる
    add_android_openssl_libraries(QllamaTalkApp)
    target_link_libraries(QllamaTalkApp PRIVATE ${android_extra_libs})

    # 3) llama の .so をインポートライブラリに設定
    set(LLAMA_SO_PATH "${LLAMA_DYNAMIC_LIB_FILE_DIR}/libllama.so")
    file(GLOB GGML_SO_PATHS "${GGML_DYNAMIC_LIB_FILE_DIR}/libggml*.so")

    # llama
    add_library(llama SHARED IMPORTED)
    set_target_properties(llama PROPERTIES
        IMPORTED_LOCATION "${LLAMA_SO_PATH}"
        ANDROID_PACKAGE_SHARED_LIBS ON
    )
    target_link_libraries(QllamaTalkApp PRIVATE llama)

    # ggml (複数ファイルをループで追加)
    list(LENGTH GGML_SO_PATHS GGML_COUNT)
    if(GGML_COUNT GREATER 0)
        foreach(ggml_file IN LISTS GGML_SO_PATHS)
            get_filename_component(ggml_name ${ggml_file} NAME_WE)
            string(REPLACE "-" "_" ggml_target_name ${ggml_name})
            add_library(${ggml_target_name} SHARED IMPORTED)
            set_target_properties(${ggml_target_name} PROPERTIES
                IMPORTED_LOCATION "${ggml_file}"
                ANDROID_PACKAGE_SHARED_LIBS ON
            )
            target_link_libraries(QllamaTalkApp PRIVATE ${ggml_target_name})
        endforeach()
    endif()

    # whisper
    add_library(whisper SHARED IMPORTED)
    set_target_properties(whisper PROPERTIES
        IMPORTED_LOCATION "${WHISPER_LIB_FILE_DIR}/libwhisper.so"
        ANDROID_PACKAGE_SHARED_LIBS ON
    )
    target_link_libraries(QllamaTalkApp PRIVATE whisper)

    message(STATUS "IMPORTED llama => ${LLAMA_SO_PATH}")
    message(STATUS "IMPORTED ggml  => ${GGML_SO_PATHS}")
    message(STATUS "IMPORTED whisper => ${WHISPER_LIB_FILE_DIR}/libwhisper.so")
endif()
