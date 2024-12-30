#
# llama_setup.cmake
#
# llama.cpp のサブモジュールを設定し、指定されたコミットへチェックアウトし、
# すべての GPU バックエンドを無条件で有効化してビルド後にインストールするサンプルです。
# (Sets up the llama.cpp submodule, checks out a specified commit,
#  enables all GPU backends unconditionally, and installs after building.)
#
# 通常、CMake 実行のたびに何度もビルドが走らないよう工夫しています。
# (We try to avoid rebuilding every time CMake runs.)

# ----------------------------------------------------------------------------
# 1) llama.cpp のサブモジュールのパスを定義
#    (Define the path for the llama.cpp submodule)
# ----------------------------------------------------------------------------
set(LLAMA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/llama.cpp")

# ----------------------------------------------------------------------------
# 2) llama.cpp サブモジュールを更新・特定コミットへチェックアウト
#    (Update the submodule and checkout the specific commit for llama.cpp)
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
# 3) すべての GPU バックエンドを無条件で有効化
#    (Unconditionally enable all GPU backends)
# ----------------------------------------------------------------------------
set(LLAMA_BUILD_DIR "${LLAMA_SOURCE_DIR}/build")

# ----------------------------------------------------------------------------
# 4) llama.cpp をビルドしてインストール (一度だけ)
#    (Build llama.cpp once and install it)
# ----------------------------------------------------------------------------
set(LLAMA_INSTALL_DIR "${CMAKE_SOURCE_DIR}/llamacpp_install")
set(LLAMA_BUILD_STAMP "${LLAMA_BUILD_DIR}/.llama_build_done")

if(NOT EXISTS "${LLAMA_BUILD_STAMP}")
    message(STATUS "llama_setup: Llama library not found in build dir -> configuring & building")

    file(MAKE_DIRECTORY "${LLAMA_BUILD_DIR}")

    execute_process(
        COMMAND "${CMAKE_COMMAND}"
                -B "${LLAMA_BUILD_DIR}"
                -S "${LLAMA_SOURCE_DIR}"
                -DGGML_METAL=ON     # Metal (Apple GPU)
                -DGGML_CUDA=ON      # CUDA (NVIDIA GPU)
                -DGGML_HIP=ON       # HIP (AMD GPU)
                -DGGML_VULKAN=ON    # Vulkan
                -DGGML_OPENCL=ON    # OpenCL
                -DGGML_MUSA=ON      # MUSA (Moore Threads)
                -DGGML_CANN=ON      # CANN (Ascend NPU)
                # -DGGML_SYCL=ON    # 必要であれば SYCL も ON (Enable SYCL if needed)
                -DCMAKE_INSTALL_PREFIX="${LLAMA_INSTALL_DIR}"
        WORKING_DIRECTORY "${LLAMA_BUILD_DIR}"
    )
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${LLAMA_BUILD_DIR}" --target install
        WORKING_DIRECTORY "${LLAMA_BUILD_DIR}"
    )

    file(WRITE "${LLAMA_BUILD_STAMP}" "Llama build success at ${CMAKE_SYSTEM_NAME}")
else()
    message(STATUS "llama_setup: Llama library is already built -> skipping rebuild")
endif()

# ----------------------------------------------------------------------------
# 5) 上位 CMake からリンクしやすくするために、インストールディレクトリを返す
#    (Set library/install directories to be used in the parent CMake)
# ----------------------------------------------------------------------------
set(LLAMA_LIB_DIR "${LLAMA_INSTALL_DIR}/lib")
set(LLAMA_INCLUDE_DIR "${LLAMA_INSTALL_DIR}/include")

# ----------------------------------------------------------------------------
# 6) OS 別に、実際のビルド成果物 (.lib / .dll / .dylib / .so) のあるディレクトリを定義
#    ※ Windows ではフォルダ名を必ず「Debug」に固定
#    (Define directories containing build artifacts for each OS.
#     On Windows, always use the "Debug" folder name.)
# ----------------------------------------------------------------------------
if(WIN32)
    # Windows: フォルダ名を絶対に "Debug" に固定
    # (On Windows, force the output folder to be named "Debug".)
    set(LLAMA_LIB_FILE_DIR
        "${LLAMA_BUILD_DIR}/src/Debug"
    )
    set(GGML_LIB_FILE_DIR
        "${LLAMA_BUILD_DIR}/ggml/src/Debug"
    )

    set(LLAMA_DYNAMIC_LIB_FILE_DIR
        "${LLAMA_BUILD_DIR}/bin/Debug"
    )
    set(GGML_DYNAMIC_LIB_FILE_DIR
        "${LLAMA_BUILD_DIR}/bin/Debug"
    )
elseif(APPLE)
    # Mac: libllama.dylib -> build/src
    #      libggml-*.dylib -> build/ggml
    # (For macOS: .dylib files go to build/src or build/ggml.)
    set(LLAMA_DYNAMIC_LIB_FILE_DIR
        "${LLAMA_BUILD_DIR}/src"
    )
    set(GGML_DYNAMIC_LIB_FILE_DIR
        "${LLAMA_BUILD_DIR}/ggml"
    )
    set(LLAMA_LIB_FILE_DIR
        "${LLAMA_BUILD_DIR}/src"
    )
    set(GGML_LIB_FILE_DIR
        "${LLAMA_BUILD_DIR}/ggml"
    )
else()
    # Linux / UNIX 系: .so が build/src, build/ggml に作られる想定
    # (For Linux/UNIX: .so files in build/src or build/ggml.)
    set(LLAMA_DYNAMIC_LIB_FILE_DIR
        "${LLAMA_BUILD_DIR}/src"
    )
    set(GGML_DYNAMIC_LIB_FILE_DIR
        "${LLAMA_BUILD_DIR}/ggml"
    )
    set(LLAMA_LIB_FILE_DIR
        "${LLAMA_BUILD_DIR}/src"
    )
    set(GGML_LIB_FILE_DIR
        "${LLAMA_BUILD_DIR}/ggml"
    )
endif()

# ----------------------------------------------------------------------------
# 7) すべての定義済み変数をデバッグ出力
#    (Print all defined variables for debugging)
# ----------------------------------------------------------------------------
message(STATUS "----- Llama Setup Variables (Debug Print) -----")
message(STATUS "LLAMA_SOURCE_DIR               = ${LLAMA_SOURCE_DIR}")
message(STATUS "LLAMA_TARGET_COMMIT            = ${LLAMA_TARGET_COMMIT}")
message(STATUS "LLAMA_CURRENT_COMMIT           = ${LLAMA_CURRENT_COMMIT}")
message(STATUS "LLAMA_BUILD_DIR                = ${LLAMA_BUILD_DIR}")
message(STATUS "LLAMA_INSTALL_DIR              = ${LLAMA_INSTALL_DIR}")
message(STATUS "LLAMA_BUILD_STAMP              = ${LLAMA_BUILD_STAMP}")
message(STATUS "LLAMA_LIB_DIR                  = ${LLAMA_LIB_DIR}")
message(STATUS "LLAMA_INCLUDE_DIR              = ${LLAMA_INCLUDE_DIR}")
message(STATUS "LLAMA_LIB_FILE_DIR             = ${LLAMA_LIB_FILE_DIR}")
message(STATUS "GGML_LIB_FILE_DIR              = ${GGML_LIB_FILE_DIR}")
message(STATUS "LLAMA_DYNAMIC_LIB_FILE_DIR     = ${LLAMA_DYNAMIC_LIB_FILE_DIR}")
message(STATUS "GGML_DYNAMIC_LIB_FILE_DIR      = ${GGML_DYNAMIC_LIB_FILE_DIR}")
message(STATUS "------------------------------------------------")
