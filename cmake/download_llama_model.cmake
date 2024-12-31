#
# download_llama_model.cmake
#
# 指定された URL から Llama モデルファイルをダウンロードし、
# CMAKE_CURRENT_SOURCE_DIR/content/llama_models に配置するサンプル。
#

cmake_minimum_required(VERSION 3.16)

# URL とファイル名を条件によって切り替える
if(IOS)
    # iOS用: より小さい 3B モデル
    # set(LLAMA_MODEL_URL "https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q6_K_L.gguf?download=true")
    # set(LLAMA_MODEL_NAME "Llama-3.2-3B-Instruct-Q6_K_L.gguf")
    # set(LLAMA_MODEL_URL "https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_0_8_8.gguf?download=true")
    # set(LLAMA_MODEL_NAME "Llama-3.2-3B-Instruct-Q4_0_8_8.gguf")
    set(LLAMA_MODEL_URL "https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-IQ3_M.gguf?download=true")
    set(LLAMA_MODEL_NAME "Llama-3.2-3B-Instruct-IQ3_M.gguf")
else()
    # 非iOS用: これまでの 8B モデル
    set(LLAMA_MODEL_URL "https://huggingface.co/Triangle104/Llama-3.1-8B-Open-SFT-Q4_K_M-GGUF/resolve/main/llama-3.1-8b-open-sft-q4_k_m.gguf?download=true")
    set(LLAMA_MODEL_NAME "llama-3.1-8b-open-sft-q4_k_m.gguf")
endif()

set(LLAMA_MODEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/content/llama_models")
set(LLAMA_MODEL_OUTPUT_PATH "${LLAMA_MODEL_DIR}/${LLAMA_MODEL_NAME}")

message(STATUS "----- Llama Model Download Setup -----")
message(STATUS "Model URL     : ${LLAMA_MODEL_URL}")
message(STATUS "Output Folder : ${LLAMA_MODEL_DIR}")
message(STATUS "Output File   : ${LLAMA_MODEL_OUTPUT_PATH}")
message(STATUS "--------------------------------------")

# ダウンロード先ディレクトリを作成 (Create the download directory if it doesn't exist)
file(MAKE_DIRECTORY "${LLAMA_MODEL_DIR}")

# すでにファイルがあるかどうかチェック (Check if the file already exists)
if(EXISTS "${LLAMA_MODEL_OUTPUT_PATH}")
    message(STATUS "Llama model file already exists at: ${LLAMA_MODEL_OUTPUT_PATH}")
else()
    message(STATUS "Downloading llama model from: ${LLAMA_MODEL_URL}")

    # file(DOWNLOAD ...) を使用してダウンロード
    file(DOWNLOAD
        "${LLAMA_MODEL_URL}"
        "${LLAMA_MODEL_OUTPUT_PATH}"
        SHOW_PROGRESS
        STATUS DOWNLOAD_STATUS
    )

    list(GET DOWNLOAD_STATUS 0 DOWNLOAD_RESULT_CODE)
    list(GET DOWNLOAD_STATUS 1 DOWNLOAD_ERROR_MESSAGE)

    # ファイルダウンロード結果をチェック
    if(NOT DOWNLOAD_RESULT_CODE EQUAL 0)
        message(FATAL_ERROR "Failed to download Llama model. Error: ${DOWNLOAD_ERROR_MESSAGE}")
    else()
        message(STATUS "Llama model downloaded successfully.")
    endif()
endif()
