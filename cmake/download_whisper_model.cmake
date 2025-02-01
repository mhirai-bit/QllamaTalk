#
# download_whisper_model.cmake
#
# 指定された URL から Whisper モデルファイルをダウンロードし、
# CMAKE_CURRENT_SOURCE_DIR/content/whisper_models に配置する。
# (Download a Whisper model file from the specified URL, and place it under
# CMAKE_CURRENT_SOURCE_DIR/content/whisper_models.)

cmake_minimum_required(VERSION 3.18)
set(WHISPER_USE_COREML OFF)
# URL とファイル名を条件によって切り替える
# (Switch the download URL and model file name depending on the platform)
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(WHISPER_USE_COREML ON)
    message(STATUS "Target platform is Apple Silicon (arm64).")
    message(STATUS "Use CoreML model for Apple platforms")
    if(IOS)
        set(WHISPER_MODEL_NAME "ggml-base.bin")
        set(WHISPER_ZIP_FILE_NAME "ggml-base-encoder.mlmodelc.zip")
        set(WHISPER_EXTRACTED_MODEL_NAME "ggml-base-encoder.mlmodelc")
    else()
        set(WHISPER_MODEL_NAME "ggml-medium.bin")
        set(WHISPER_ZIP_FILE_NAME "ggml-medium-encoder.mlmodelc.zip")
        set(WHISPER_EXTRACTED_MODEL_NAME "ggml-medium-encoder.mlmodelc")
    endif()
endif()
if(APPLE)
    message(STATUS "Target platform is Apple (x86_64).")
    if(IOS)
        set(WHISPER_DOWNLOAD_URL "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin?download=true")
        set(WHISPER_MODEL_NAME "ggml-base.bin")
    else()
        set(WHISPER_DOWNLOAD_URL "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.bin?download=true")
        set(WHISPER_MODEL_NAME "ggml-medium.bin")
    endif()
elseif(ANDROID)
    # Androidは初回起動時にモデルをインターネットからダウンロードする)
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
if(WHISPER_USE_COREML)
set(WHISPER_MODEL_ZIP_FILE_PATH "${WHISPER_MODEL_DIR}/CoreML/${WHISPER_ZIP_FILE_NAME}")
endif()

message(STATUS "----- Whisper Model Download Setup -----")
message(STATUS "Model URL     : ${WHISPER_MODEL_URL}")
message(STATUS "Output Folder : ${WHISPER_MODEL_DIR}")
message(STATUS "Output File   : ${WHISPER_MODEL_OUTPUT_PATH}")
if(WHISPER_USE_COREML)
message(STATUS "Output Zip File   : ${WHISPER_MODEL_ZIP_FILE_PATH}")
endif()
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

# CoreML モデルの場合は解凍
if(WHISPER_USE_COREML)
    file(ARCHIVE_EXTRACT
        INPUT "${WHISPER_MODEL_ZIP_FILE_PATH}"
        DESTINATION "${WHISPER_MODEL_DIR}"
    )
set(WHISPER_CORE_ML_MODEL_PATH "${WHISPER_MODEL_DIR}/${WHISPER_EXTRACTED_MODEL_NAME}")
message(STATUS "Extracted CoreML model: ${WHISPER_CORE_ML_MODEL_PATH}")
endif()

#
# モデルを QllamaTalkApp 実行ファイルの横にコピーß
#    (ここでは、QllamaTalkApp という名称のターゲットが存在すると仮定)
#
# 例: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} に実行ファイルが出る場合や
#     add_executable(QllamaTalkApp ...) 後に TGT_FILE_DIR が利用できる
#
# ※ 上位で QllamaTalkApp を定義している必要があります
#
if(WHISPER_USE_COREML)
add_custom_command(TARGET QllamaTalkApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${WHISPER_CORE_ML_MODEL_PATH}"
        "$<TARGET_FILE_DIR:QllamaTalkApp>/${WHISPER_EXTRACTED_MODEL_NAME}"
    COMMENT "Copying CoreML Whisper model folder to QllamaTalkApp binary directory"
)
endif()

add_custom_command(TARGET QllamaTalkApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${WHISPER_MODEL_OUTPUT_PATH}"
    "$<TARGET_FILE_DIR:QllamaTalkApp>/${WHISPER_MODEL_NAME}"
    COMMENT "Copying Whisper model to QllamaTalkApp binary directory"
)
