#
# download_whisper_model.cmake
#
# 指定された URL から Whisper モデルファイルをダウンロードし、
# CMAKE_CURRENT_SOURCE_DIR/content/whisper_models に配置する。
# (Download a Whisper model file from the specified URL, and place it under
# CMAKE_CURRENT_SOURCE_DIR/content/whisper_models.)

cmake_minimum_required(VERSION 3.16)

# URL とファイル名を条件によって切り替える
# (Switch the download URL and model file name depending on the platform)
if(IOS OR ANDROID)
    # iOS: より小さいggml-base.binモデル(Androidはサイズが1.6GBのモデルでもビルドに失敗するので、初回起動時にダウンロードする)
    # (For iOS: Use a smaller ggml-base model)
    set(WHISPER_DOWNLOAD_URL "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin?download=true")
    set(WHISPER_MODEL_NAME "ggml-base.bin")
else()
    # 非モバイル用: ggml-medium.binモデル
    # (For non-iOS: Use the ggml-medium.bin model)
    set(WHISPER_DOWNLOAD_URL "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.bin?download=true")
    set(WHISPER_MODEL_NAME "ggml-medium.bin")
endif()
set(WHISPER_MODEL_URL ${WHISPER_DOWNLOAD_URL})

# 出力先パスの設定
# (Set the destination path for the downloaded model)
set(WHISPER_MODEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/content/whisper_models")
set(WHISPER_MODEL_OUTPUT_PATH "${WHISPER_MODEL_DIR}/${WHISPER_MODEL_NAME}")

message(STATUS "----- Whisper Model Download Setup -----")
message(STATUS "Model URL     : ${WHISPER_MODEL_URL}")
message(STATUS "Output Folder : ${WHISPER_MODEL_DIR}")
message(STATUS "Output File   : ${WHISPER_MODEL_OUTPUT_PATH}")
message(STATUS "--------------------------------------")

# ダウンロード先ディレクトリを作成 (Create the download directory if it doesn't exist)
file(MAKE_DIRECTORY "${WHISPER_MODEL_DIR}")

# すでにファイルがあるかどうかチェック (Check if the file already exists)
if(EXISTS "${WHISPER_MODEL_OUTPUT_PATH}")
    message(STATUS "Whisper model file already exists at: ${WHISPER_MODEL_OUTPUT_PATH}")
else()
    message(STATUS "Downloading whisper model from: ${WHISPER_MODEL_URL}")

    # file(DOWNLOAD ...) を使用してダウンロード
    # (Use file(DOWNLOAD ...) to download the model)
    file(DOWNLOAD
        "${WHISPER_MODEL_URL}"
        "${WHISPER_MODEL_OUTPUT_PATH}"
        SHOW_PROGRESS
        STATUS DOWNLOAD_STATUS
    )

    list(GET DOWNLOAD_STATUS 0 DOWNLOAD_RESULT_CODE)
    list(GET DOWNLOAD_STATUS 1 DOWNLOAD_ERROR_MESSAGE)

    # ファイルダウンロード結果をチェック
    # (Check the result of the file download)
    if(NOT DOWNLOAD_RESULT_CODE EQUAL 0)
        message(FATAL_ERROR "Failed to download Whisper model. Error: ${DOWNLOAD_ERROR_MESSAGE}")
    else()
        message(STATUS "Whisper model downloaded successfully.")
    endif()
endif()
