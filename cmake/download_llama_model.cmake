#
# download_llama_model.cmake
#
# 指定された URL から Llama モデルファイルをダウンロードし、
# CMAKE_CURRENT_SOURCE_DIR/content/llama_models に配置する。
# (Download a Llama model file from the specified URL, and place it under
# CMAKE_CURRENT_SOURCE_DIR/content/llama_models.)

cmake_minimum_required(VERSION 3.16)

# URL とファイル名を条件によって切り替える
# (Switch the download URL and model file name depending on the platform)
if(IOS)
    # iOS用: より小さい 3B モデル
    # (For iOS: Use a smaller 3B model)
    set(LLAMA_MODEL_URL "https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-IQ3_M.gguf?download=true")
    set(LLAMA_MODEL_NAME "Llama-3.2-3B-Instruct-IQ3_M.gguf")
else()
    # 非iOS用: これまでの 8B モデル
    # (For non-iOS: Use the existing 8B model)
    set(LLAMA_MODEL_URL "https://huggingface.co/Triangle104/Llama-3.1-8B-Open-SFT-Q4_K_M-GGUF/resolve/main/llama-3.1-8b-open-sft-q4_k_m.gguf?download=true")
    set(LLAMA_MODEL_NAME "llama-3.1-8b-open-sft-q4_k_m.gguf")
endif()

# 出力先パスの設定
# (Set the destination path for the downloaded model)
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
    # (Use file(DOWNLOAD ...) to download the model)
    file(DOWNLOAD
        "${LLAMA_MODEL_URL}"
        "${LLAMA_MODEL_OUTPUT_PATH}"
        SHOW_PROGRESS
        STATUS DOWNLOAD_STATUS
    )

    list(GET DOWNLOAD_STATUS 0 DOWNLOAD_RESULT_CODE)
    list(GET DOWNLOAD_STATUS 1 DOWNLOAD_ERROR_MESSAGE)

    # ファイルダウンロード結果をチェック
    # (Check the result of the file download)
    if(NOT DOWNLOAD_RESULT_CODE EQUAL 0)
        message(FATAL_ERROR "Failed to download Llama model. Error: ${DOWNLOAD_ERROR_MESSAGE}")
    else()
        message(STATUS "Llama model downloaded successfully.")
    endif()
endif()
