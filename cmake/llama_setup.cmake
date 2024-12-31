#
# llama_setup.cmake
#
# llama.cpp のサブモジュールを設定し、指定されたコミットへチェックアウトし、
# すべての GPU バックエンドを（例示として）無条件で有効化してビルドのみ行うサンプルです。
# インストール (make install / cmake --build install) は一切しません。
#
# 通常、CMake 実行のたびに何度もビルドが走らないよう工夫しています。
#

# ----------------------------------------------------------------------------
# 1) llama.cpp サブモジュールのパス
# ----------------------------------------------------------------------------
set(LLAMA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/llama.cpp")

# ----------------------------------------------------------------------------
# 2) サブモジュール初期化 & 特定コミットへチェックアウト
# ----------------------------------------------------------------------------
if(NOT EXISTS "${LLAMA_SOURCE_DIR}/.git")
    message(STATUS "llama_setup: Submodule 'llama.cpp' does not look initialized -> updating submodule")
    execute_process(
        COMMAND git submodule update --init --checkout --recursive 3rdparty/llama.cpp
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE SUBMODULE_UPDATE_RESULT
    )
    if(NOT ${SUBMODULE_UPDATE_RESULT} EQUAL 0)
        message(FATAL_ERROR "llama_setup: Failed to update submodule 'llama.cpp'")
    endif()
endif()

# チェックアウトしたいコミット (The commit to be checked out)
set(LLAMA_TARGET_COMMIT "30caac3a68a54de8396b21e20ba972554c587230")

execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY "${LLAMA_SOURCE_DIR}"
    OUTPUT_VARIABLE LLAMA_CURRENT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT LLAMA_CURRENT_COMMIT STREQUAL LLAMA_TARGET_COMMIT)
    message(STATUS "llama_setup: Checking out commit ${LLAMA_TARGET_COMMIT} in llama.cpp")
    execute_process(
        COMMAND git checkout ${LLAMA_TARGET_COMMIT}
        WORKING_DIRECTORY "${LLAMA_SOURCE_DIR}"
        RESULT_VARIABLE SUBMODULE_CHECKOUT_RESULT
    )
    if(NOT ${SUBMODULE_CHECKOUT_RESULT} EQUAL 0)
        message(FATAL_ERROR "llama_setup: Failed to checkout commit ${LLAMA_TARGET_COMMIT}")
    endif()
endif()

# ----------------------------------------------------------------------------
# 3) GPU バックエンドを無条件で有効化する例（実際の要件に合わせて修正）
# ----------------------------------------------------------------------------
#   実際に使う GPU オプションだけ ON にするか、不要なら削除してください。

# ----------------------------------------------------------------------------
# 4) llama.cpp をビルド (インストールはしない)
# ----------------------------------------------------------------------------
#
# iOS の場合は build_iOS、それ以外は build フォルダを使う例。
# ここでインストールはせず、ビルドだけ行います。
#

if(APPLE)
    # iOSかどうかを判定する適当な条件（下は例）
    # CMAKE_OSX_SYSROOT に iPhoneOS が含まれていれば iOS と見なす
    if(IOS)
        set(LLAMA_BUILD_DIR "${LLAMA_SOURCE_DIR}/build_iOS")
        set(IOS_BUILD_OPTIONS
            -DCMAKE_SYSTEM_NAME=iOS
            -DCMAKE_OSX_ARCHITECTURES=arm64
            -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0
            -DBUILD_SHARED_LIBS=OFF  # iOS用に静的ライブラリを例示
            -DLLAMA_BUILD_TESTS=OFF
            -DLLAMA_BUILD_EXAMPLES=OFF
        )
        # iOS向けMetalは現状 llama.cpp でサポートしていないためオフ
        set(METAL_OPTION "-DGGML_METAL=ON")
    else()
        # macOS
        set(LLAMA_BUILD_DIR "${LLAMA_SOURCE_DIR}/build")
        # Metal を有効化
        set(METAL_OPTION "-DGGML_METAL=ON")
        set(IOS_BUILD_OPTIONS "")
    endif()
else()
    # Windows / Linux / etc.
    set(LLAMA_BUILD_DIR "${LLAMA_SOURCE_DIR}/build")
    set(METAL_OPTION "")
    set(IOS_BUILD_OPTIONS "")
endif()

# stamp ファイルを置いてビルド済みかどうかの簡易チェックを行う
set(LLAMA_BUILD_STAMP "${LLAMA_BUILD_DIR}/.llama_build_done")

if(NOT EXISTS "${LLAMA_BUILD_STAMP}")
    message(STATUS "llama_setup: Llama library not found in build dir -> configuring & building")

    file(MAKE_DIRECTORY "${LLAMA_BUILD_DIR}")

    # (1) CMake configure
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
                -B "${LLAMA_BUILD_DIR}"
                -S "${LLAMA_SOURCE_DIR}"
                ${IOS_BUILD_OPTIONS}
                ${METAL_OPTION}
                # ここで必要に応じて CUDA / HIP / Vulkan / SYCL などのオプションを設定
        WORKING_DIRECTORY "${LLAMA_BUILD_DIR}"
    )

    # (2) ビルドのみ (install しない)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${LLAMA_BUILD_DIR}"
        WORKING_DIRECTORY "${LLAMA_BUILD_DIR}"
    )

    # stamp ファイル作成
    file(WRITE "${LLAMA_BUILD_STAMP}" "Llama build success at ${CMAKE_SYSTEM_NAME}")
else()
    message(STATUS "llama_setup: Llama library is already built -> skipping rebuild")
endif()

# ----------------------------------------------------------------------------
# 5) 上位 CMake に返す変数定義
#    (実際にリンクするライブラリへのパスなど)
# ----------------------------------------------------------------------------

# ※ ここではインストールディレクトリではなく、ビルドディレクトリ内を直接使う前提
# llama.cpp の CMakeLists.txtでは、生成物が "build*/src/" に出てくる想定

if(WIN32)
    # Windows: フォルダ名を絶対に "Debug" に固定 (llama.cppのCMakeLists.txt上の設定)
    set(LLAMA_LIB_FILE_DIR             "${LLAMA_BUILD_DIR}/src/Debug")
    set(GGML_LIB_FILE_DIR             "${LLAMA_BUILD_DIR}/ggml/src/Debug")
    set(LLAMA_DYNAMIC_LIB_FILE_DIR    "${LLAMA_BUILD_DIR}/bin/Debug")
    set(GGML_DYNAMIC_LIB_FILE_DIR     "${LLAMA_BUILD_DIR}/bin/Debug")
elseif(APPLE)
    # macOS/iOS: libllama.dylib -> build*/src
    #            libggml-*.dylib -> build*/ggml/src
    set(LLAMA_LIB_FILE_DIR             "${LLAMA_BUILD_DIR}/src")
    set(GGML_LIB_FILE_DIR             "${LLAMA_BUILD_DIR}/ggml/src")
    set(LLAMA_DYNAMIC_LIB_FILE_DIR    "${LLAMA_BUILD_DIR}/src")
    set(GGML_DYNAMIC_LIB_FILE_DIR     "${LLAMA_BUILD_DIR}/ggml/src")
else()
    # Linux / UNIX 系
    set(LLAMA_LIB_FILE_DIR             "${LLAMA_BUILD_DIR}/src")
    set(GGML_LIB_FILE_DIR             "${LLAMA_BUILD_DIR}/ggml/src")
    set(LLAMA_DYNAMIC_LIB_FILE_DIR    "${LLAMA_BUILD_DIR}/src")
    set(GGML_DYNAMIC_LIB_FILE_DIR     "${LLAMA_BUILD_DIR}/ggml/src")
endif()

# include/ は build*/ には含まれないため、必要なら llama.cpp/ggml/include/ を参照
# (llama.cpp では top-level include ディレクトリにインクルードヘッダをまとめていないので、
#  3rdparty/llama.cpp/include, 3rdparty/llama.cpp/ggml/include, などを使う場合も)
#
# 例:
set(LLAMA_INCLUDE_DIR "${LLAMA_SOURCE_DIR}/include")
set(GGML_INCLUDE_DIR  "${LLAMA_SOURCE_DIR}/ggml/include")

# ----------------------------------------------------------------------------
# デバッグ出力
# ----------------------------------------------------------------------------
message(STATUS "----- Llama Setup Variables (Debug Print) -----")
message(STATUS "LLAMA_SOURCE_DIR               = ${LLAMA_SOURCE_DIR}")
message(STATUS "LLAMA_TARGET_COMMIT            = ${LLAMA_TARGET_COMMIT}")
message(STATUS "LLAMA_CURRENT_COMMIT           = ${LLAMA_CURRENT_COMMIT}")
message(STATUS "LLAMA_BUILD_DIR                = ${LLAMA_BUILD_DIR}")
message(STATUS "LLAMA_BUILD_STAMP              = ${LLAMA_BUILD_STAMP}")
message(STATUS "LLAMA_LIB_FILE_DIR             = ${LLAMA_LIB_FILE_DIR}")
message(STATUS "GGML_LIB_FILE_DIR              = ${GGML_LIB_FILE_DIR}")
message(STATUS "LLAMA_DYNAMIC_LIB_FILE_DIR     = ${LLAMA_DYNAMIC_LIB_FILE_DIR}")
message(STATUS "GGML_DYNAMIC_LIB_FILE_DIR      = ${GGML_DYNAMIC_LIB_FILE_DIR}")
message(STATUS "LLAMA_INCLUDE_DIR              = ${LLAMA_INCLUDE_DIR}")
message(STATUS "GGML_INCLUDE_DIR               = ${GGML_INCLUDE_DIR}")
message(STATUS "------------------------------------------------")
