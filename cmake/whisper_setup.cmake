#
# whisper_setup.cmake
#
# whisper.cpp のサブモジュールを設定し、指定されたコミットへチェックアウトし、
# シンプルにビルドのみ行うサンプルです。
# さらに、WHISPER_MODEL_PATH にあるモデルを QllamaTalkApp 実行ファイルの横にコピーし、
# ファイル名を C++ から参照できるように定義する。
#

cmake_minimum_required(VERSION 3.15)

# ----------------------------------------------------------------------------
# 1) whisper.cpp サブモジュールのパス
# ----------------------------------------------------------------------------
set(WHISPER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/whisper.cpp")

# ----------------------------------------------------------------------------
# 2) サブモジュール初期化 & 特定コミットへチェックアウト
# ----------------------------------------------------------------------------
if(NOT EXISTS "${WHISPER_SOURCE_DIR}/.git")
    message(STATUS "whisper_setup: Submodule 'whisper.cpp' does not look initialized -> updating submodule")
    execute_process(
        COMMAND git submodule update --init --checkout --recursive 3rdparty/whisper.cpp
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE SUBMODULE_UPDATE_RESULT
    )
    if(NOT ${SUBMODULE_UPDATE_RESULT} EQUAL 0)
        message(FATAL_ERROR "whisper_setup: Failed to update submodule 'whisper.cpp'")
    endif()
endif()

set(WHISPER_TARGET_COMMIT "7d55637f0bc8ff1b405f6f18c5c04fe39e4ad389")

execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY "${WHISPER_SOURCE_DIR}"
    OUTPUT_VARIABLE WHISPER_CURRENT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT WHISPER_CURRENT_COMMIT STREQUAL WHISPER_TARGET_COMMIT)
    message(STATUS "whisper_setup: Checking out commit ${WHISPER_TARGET_COMMIT} in whisper.cpp")
    execute_process(
        COMMAND git checkout ${WHISPER_TARGET_COMMIT}
        WORKING_DIRECTORY "${WHISPER_SOURCE_DIR}"
        RESULT_VARIABLE SUBMODULE_CHECKOUT_RESULT
    )
    if(NOT ${SUBMODULE_CHECKOUT_RESULT} EQUAL 0)
        message(FATAL_ERROR "whisper_setup: Failed to checkout commit ${WHISPER_TARGET_COMMIT}")
    endif()
endif()

# ----------------------------------------------------------------------------
# 3) whisper.cpp に対するビルドオプション例
# ----------------------------------------------------------------------------
set(WHISPER_OPTION_BUILD_EXAMPLES "-DWHISPER_BUILD_EXAMPLES=OFF")
set(WHISPER_OPTION_BUILD_TESTS    "-DWHISPER_BUILD_TESTS=OFF")
set(WHISPER_OPTION_NO_INSTALL     "-DWHISPER_NO_INSTALL=ON")

# ----------------------------------------------------------------------------
# 4) iOS / macOS / Android / その他 でオプションを分岐
# ----------------------------------------------------------------------------
set(IOS_BUILD_OPTIONS "")
set(ANDROID_BUILD_OPTIONS "")
set(COREML_OPTION "")
set(WHISPER_BUILD_DIR "${WHISPER_SOURCE_DIR}/build")

if(APPLE)
    if(IOS)
        set(WHISPER_BUILD_DIR "${WHISPER_SOURCE_DIR}/build_iOS")
        set(IOS_BUILD_OPTIONS
            -DCMAKE_SYSTEM_NAME=iOS
            -DCMAKE_OSX_ARCHITECTURES=arm64
            -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0
            -DBUILD_SHARED_LIBS=OFF
        )
        if(WHISPER_USE_COREML)
            set(WHISPER_BUILD_DIR "${WHISPER_SOURCE_DIR}/build_iOS_CoreML")
            set(COREML_OPTION "-DWHISPER_COREML=1")
        endif()
    else()
        # macOS (Apple Silicon など)
        set(WHISPER_BUILD_DIR "${WHISPER_SOURCE_DIR}/build_macOS")
        if(WHISPER_USE_COREML)
            set(COREML_OPTION "-DWHISPER_COREML=1")
            set(WHISPER_BUILD_DIR "${WHISPER_SOURCE_DIR}/build_macOS_CoreML")
        endif()
    endif()
elseif(ANDROID)
    set(WHISPER_BUILD_DIR "${WHISPER_SOURCE_DIR}/build_Android")
    set(ANDROID_BUILD_OPTIONS
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DANDROID_ABI=${ANDROID_ABI}
        -DANDROID_PLATFORM=${ANDROID_PLATFORM}
        -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        -DBUILD_SHARED_LIBS=ON
    )
else()
    # Windows / Linux / etc.
    set(WHISPER_BUILD_DIR "${WHISPER_SOURCE_DIR}/build")
endif()

# ----------------------------------------------------------------------------
# 5) whisper.cpp をビルド
# ----------------------------------------------------------------------------
set(WHISPER_BUILD_STAMP "${WHISPER_BUILD_DIR}/.whisper_build_done")

if(NOT EXISTS "${WHISPER_BUILD_STAMP}")
    message(STATUS "whisper_setup: Whisper library not found in build dir -> configuring & building")

    file(MAKE_DIRECTORY "${WHISPER_BUILD_DIR}")

    # (1) configure
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
                -B "${WHISPER_BUILD_DIR}"
                -S "${WHISPER_SOURCE_DIR}"
                -G "${CMAKE_GENERATOR}"
                ${IOS_BUILD_OPTIONS}
                ${ANDROID_BUILD_OPTIONS}
                ${COREML_OPTION}
                ${WHISPER_OPTION_BUILD_EXAMPLES}
                ${WHISPER_OPTION_BUILD_TESTS}
                ${WHISPER_OPTION_NO_INSTALL}
        WORKING_DIRECTORY "${WHISPER_BUILD_DIR}"
    )

    # (2) build only
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${WHISPER_BUILD_DIR}" --config Release
        WORKING_DIRECTORY "${WHISPER_BUILD_DIR}"
    )

    file(WRITE "${WHISPER_BUILD_STAMP}" "Whisper build success at ${CMAKE_SYSTEM_NAME}")
else()
    message(STATUS "whisper_setup: Whisper library is already built -> skipping rebuild")
endif()

# ----------------------------------------------------------------------------
# 6) 上位 CMake に返す変数定義 (ライブラリパス, インクルードパス など)
# ----------------------------------------------------------------------------
if(WIN32)
    set(WHISPER_LIB_FILE_DIR  "${WHISPER_BUILD_DIR}/bin")
else()
    set(WHISPER_LIB_FILE_DIR  "${WHISPER_BUILD_DIR}/src")
endif()
set(WHISPER_INCLUDE_DIR   "${WHISPER_SOURCE_DIR}/include")
if(IOS)
    set(WHISPER_STATIC_LIB_DIR "${WHISPER_BUILD_DIR}/src/Release-iphoneos")
endif()

message(STATUS "----- Whisper Setup Variables (Debug Print) -----")
message(STATUS "WHISPER_SOURCE_DIR        = ${WHISPER_SOURCE_DIR}")
message(STATUS "WHISPER_TARGET_COMMIT     = ${WHISPER_TARGET_COMMIT}")
message(STATUS "WHISPER_CURRENT_COMMIT    = ${WHISPER_CURRENT_COMMIT}")
message(STATUS "WHISPER_BUILD_DIR         = ${WHISPER_BUILD_DIR}")
message(STATUS "WHISPER_BUILD_STAMP       = ${WHISPER_BUILD_STAMP}")
message(STATUS "WHISPER_LIB_FILE_DIR      = ${WHISPER_LIB_FILE_DIR}")
message(STATUS "WHISPER_INCLUDE_DIR       = ${WHISPER_INCLUDE_DIR}")
message(STATUS "WHISPER_USE_COREML        = ${WHISPER_USE_COREML}")
message(STATUS "WHISPER_MODEL_OUTPUT_PATH = ${WHISPER_MODEL_OUTPUT_PATH}")
message(STATUS "WHISPER_MODEL_NAME        = ${WHISPER_MODEL_NAME}")
message(STATUS "WHISPER_STATIC_LIB_DIR    = ${WHISPER_STATIC_LIB_DIR}")
message(STATUS "------------------------------------------------")
