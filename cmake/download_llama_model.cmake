#
# download_llama_model.cmake
#
# 指定された URL から Llama モデルファイルをダウンロードし、
# CMAKE_CURRENT_SOURCE_DIR/content/llama_models に配置するサンプル。
#

cmake_minimum_required(VERSION 3.16)

# ダウンロード先 URL と保存先パスを定義
# (Define the download URL and the destination path)
set(LLAMA_MODEL_URL "https://huggingface.co/Triangle104/Llama-3.1-8B-Open-SFT-Q4_K_M-GGUF/resolve/main/llama-3.1-8b-open-sft-q4_k_m.gguf?download=true")
set(LLAMA_MODEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/content/llama_models")
set(LLAMA_MODEL_NAME "llama-3.1-8b-open-sft-q4_k_m.gguf")
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

    # file(DOWNLOAD ...) を使用してダウンロード (Download using file(DOWNLOAD ...))
    # SHOW_PROGRESS は CMake 実行中に進行状況を表示
    # (SHOW_PROGRESS shows the download progress during CMake runs)
    file(DOWNLOAD 
        "${LLAMA_MODEL_URL}"
        "${LLAMA_MODEL_OUTPUT_PATH}"
        SHOW_PROGRESS
        STATUS DOWNLOAD_STATUS
    )

    list(GET DOWNLOAD_STATUS 0 DOWNLOAD_RESULT_CODE)
    list(GET DOWNLOAD_STATUS 1 DOWNLOAD_ERROR_MESSAGE)

    # ファイルダウンロード結果をチェック (Check the result of the file download)
    if(NOT DOWNLOAD_RESULT_CODE EQUAL 0)
        message(FATAL_ERROR "Failed to download Llama model. Error: ${DOWNLOAD_ERROR_MESSAGE}")
    else()
        message(STATUS "Llama model downloaded successfully.")
    endif()
endif()
