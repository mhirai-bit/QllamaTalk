qt_add_library(content STATIC)
find_package(Qt6 REQUIRED COMPONENTS Concurrent RemoteObjects WebSockets Multimedia TextToSpeech)

qt6_add_repc_replicas(content
    ../LLMRemoteServer/QtRemoteObjectsFiles/LlamaResponseGenerator.rep
)
message(STATUS "rep file path = ${CMAKE_CURRENT_SOURCE_DIR}/LLMRemoteServer/QtRemoteObjectsFiles/LlamaResponseGenerator.rep")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# ----------------------------------------------------------------------------
# C++コードからダウンロード済みの gguf モデルファイル名を参照できるようにする
# Enable referencing the downloaded gguf model file name from C++ code
# ----------------------------------------------------------------------------
target_compile_definitions(content PRIVATE
    LLAMA_MODEL_FILE=\"${LLAMA_MODEL_NAME}\"
    LLAMA_DOWNLOAD_URL=\"${LLAMA_DOWNLOAD_URL}\"
    WHISPER_MODEL_NAME=\"${WHISPER_MODEL_NAME}\"
)

message(STATUS "Check: listing directory = ${LLAMA_LIB_FILE_DIR}")
execute_process(
    COMMAND ls -la
    WORKING_DIRECTORY "${LLAMA_LIB_FILE_DIR}"
    OUTPUT_VARIABLE DIR_CONTENTS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Llama library dir content:\n${DIR_CONTENTS}")

message(STATUS "Check: listing directory = ${WHISPER_LIB_FILE_DIR}")
execute_process(
    COMMAND ls -la
    WORKING_DIRECTORY "${WHISPER_LIB_FILE_DIR}"
    OUTPUT_VARIABLE DIR_CONTENTS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Whisper library dir content:\n${DIR_CONTENTS}")

find_library(LLAMA_LIB
    NAMES llama
    PATHS "${LLAMA_LIB_FILE_DIR}"
    NO_DEFAULT_PATH
)

find_library(WHISPER_LIB
    NAMES whisper
    PATHS "${WHISPER_LIB_FILE_DIR}"
    NO_DEFAULT_PATH
)

# ----------------------------------------------------------------------------
# ここでは、プラットフォームごとに LLaMA / ggml ライブラリを検索・リンクする
# For each platform, we search and link the LLaMA / ggml libraries differently
# ----------------------------------------------------------------------------
if(NOT IOS)
    # Windows / macOS / Linux の場合は、find_library で llama と ggml を探す
    # For Windows/macOS/Linux, we find the llama and ggml libraries via find_library

    find_library(LLAMA_LIB
        NAMES llama libllama
        PATHS "${LLAMA_LIB_FILE_DIR}"
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
        # 追加のパスがあれば追記 (Add extra paths here if necessary)
    )

    find_library(GGML_LIB
        NAMES ggml libggml
        PATHS "${GGML_LIB_FILE_DIR}"
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
        # 追加のパスがあれば追記 (Add extra paths here if necessary)
    )

    # 見つからない場合はビルドを中断
    # Abort build if not found
    if(LLAMA_LIB)
        message(STATUS "Found llama library: ${LLAMA_LIB}")
    else()
        message(FATAL_ERROR "Could not find llama library (e.g. llama.lib / libllama.dylib / libllama.so)")
    endif()

    if(WHISPER_LIB)
        message(STATUS "Found whisper library: ${WHISPER_LIB}")
    else()
        message(FATAL_ERROR "Could not find whisper library (e.g. whisper.lib / libwhisper.dylib / libwhisper.so)")
    endif()

    if(GGML_LIB)
        message(STATUS "Found ggml library: ${GGML_LIB}")
    else()
        message(FATAL_ERROR "Could not find ggml library (e.g. ggml.lib / libggml.dylib / libggml.so)")
    endif()

    set(ALL_LIBS
        ${LLAMA_LIB}
        ${WHISPER_LIB}
        ${GGML_LIB}
    )
endif()

# ----------------------------------------------------------------------------
# インクルードパスを追加 (Add include directories)
# ----------------------------------------------------------------------------
target_include_directories(content PRIVATE
    ${CMAKE_SOURCE_DIR}/3rdparty/llama.cpp/include
    ${CMAKE_SOURCE_DIR}/3rdparty/llama.cpp/ggml/include
    ${WHISPER_INCLUDE_DIR}
)

# ----------------------------------------------------------------------------
# OSごとに、ライブラリファイルをビルド成果物のディレクトリへコピー
# Copy the library files to the build output directory per OS
# ----------------------------------------------------------------------------
if(WIN32)
    # Windows: .dll をコピー (Copy the .dll files)

    # llama.dll をコピー
    add_custom_command(TARGET content POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LLAMA_DYNAMIC_LIB_FILE_DIR}/llama.dll"  # 実際のファイル名に合わせてください (Adjust if the file name differs)
        "$<TARGET_FILE_DIR:QllamaTalkApp>"
        COMMENT "Copying llama.dll to QllamaTalkApp"
    )

    # ggml*.dll をコピー (Copy ggml*.dll if multiple variants exist)
    file(GLOB GGML_DLLS
        "${GGML_DYNAMIC_LIB_FILE_DIR}/ggml*.dll"
    )
    foreach(dll_file ${GGML_DLLS})
        add_custom_command(TARGET content POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${dll_file}"
            "$<TARGET_FILE_DIR:QllamaTalkApp>"
            COMMENT "Copying ggml DLLs to QllamaTalkApp"
        )
    endforeach()

    # whisper*.dll をコピー (Copy whisper*.dll if multiple variants exist)
    file(GLOB WHISPER_DLLS
        "${WHISPER_LIB_FILE_DIR}/whisper*.dll"
    )
    foreach(dll_file ${WHISPER_DLLS})
        add_custom_command(TARGET content POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${dll_file}"
            "$<TARGET_FILE_DIR:QllamaTalkApp>"
            COMMENT "Copying whisper DLLs to QllamaTalkApp"
        )
    endforeach()

elseif(APPLE)
    if(IOS)
        # iOS: llama_setup.cmake でビルドされた libllama.a と
        #      libggml*.a (静的ライブラリ) を手動で追加
        # iOS: use the static libraries built by llama_setup.cmake (libllama.a, libggml*.a)

        set(LLAMA_STATIC_PATH "${LLAMA_LIB_FILE_DIR}/libllama.a")
        if(EXISTS "${LLAMA_STATIC_PATH}")
            set(LLAMA_LIB "${LLAMA_STATIC_PATH}")
            message(STATUS "Using libllama.a for iOS: ${LLAMA_LIB}")
        else()
            message(FATAL_ERROR "Could not find llama static library (libllama.a) in ${LLAMA_LIB_FILE_DIR}")
        endif()

        # ggml 側の静的ライブラリを取得 (Including ggml-metal, ggml-blas, etc.)
        file(GLOB GGML_STATIC_LIBS
            "${GGML_LIB_FILE_DIR}/libggml*.a"
        )
        # 追加のライブラリがあれば手動でリストに追加
        list(APPEND GGML_STATIC_LIBS
            "${GGML_LIB_FILE_DIR}/ggml-metal/libggml-metal.a"
            "${GGML_LIB_FILE_DIR}/ggml-blas/libggml-blas.a"
        )

        # whisper 側の静的ライブラリを取得 (whipser.a)
        set(WHISPER_STATIC_PATH "${WHISPER_STATIC_LIB_DIR}/libwhisper.a")
        if(EXISTS "${WHISPER_STATIC_PATH}")
            message(STATUS "Using libwhisper.a for iOS: ${WHISPER_STATIC_PATH}")
        else()
            message(FATAL_ERROR "Could not find whisper static library (libwhisper.a) in ${WHISPER_STATIC_LIB_DIR}")
        endif()
        find_library(ACCELERATE Accelerate)

        if(ACCELERATE)
            message(STATUS "Found Accelerate library: ${ACCELERATE}")
        else()
            message(FATAL_ERROR "Could not find Accelerate library")
        endif()

        if(GGML_STATIC_LIBS)
            message(STATUS "Found ggml static libraries for iOS:")
            foreach(libfile ${GGML_STATIC_LIBS})
                message(STATUS "  - ${libfile}")
            endforeach()
        else()
            message(FATAL_ERROR "Could not find any libggml*.a in ${GGML_LIB_FILE_DIR}")
        endif()

        # 一つのリストにまとめる (Combine all libs into one list)
        set(ALL_LIBS
            ${LLAMA_STATIC_PATH}
            ${GGML_STATIC_LIBS}
            ${WHISPER_STATIC_PATH}
            ${ACCELERATE}
        )
    else()
        # macOS: .dylib をコピー
        # macOS: copy .dylib files

        add_custom_command(TARGET content POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LLAMA_DYNAMIC_LIB_FILE_DIR}/libllama.dylib"
            "$<TARGET_FILE_DIR:QllamaTalkApp>"
            COMMENT "Copying libllama.dylib to QllamaTalkApp"
        )

        add_custom_command(TARGET content POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WHISPER_LIB_FILE_DIR}/libwhisper*.dylib"
            "$<TARGET_FILE_DIR:QllamaTalkApp>"
            COMMENT "Copying libwhisper*.dylib to Qllama"
        )

        file(GLOB GGML_DYLIBS
            "${GGML_DYNAMIC_LIB_FILE_DIR}/libggml*.dylib"
        )
        foreach(dylib_file ${GGML_DYLIBS})
            add_custom_command(TARGET content POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${dylib_file}"
                "$<TARGET_FILE_DIR:QllamaTalkApp>"
                COMMENT "Copying libggml*.dylib to QllamaTalkApp"
            )
        endforeach()
    endif()
else()
    # Linux / UNIX / Android: .so をコピー
    # Linux/UNIX/Android: copy .so files

    add_custom_command(TARGET content POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LLAMA_DYNAMIC_LIB_FILE_DIR}/libllama.so"
        "$<TARGET_FILE_DIR:QllamaTalkApp>"
        COMMENT "Copying libllama.so to QllamaTalkApp"
    )

    file(GLOB GGML_SOS
        "${GGML_DYNAMIC_LIB_FILE_DIR}/libggml*.so"
    )
    foreach(so_file ${GGML_SOS})
        add_custom_command(TARGET content POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${so_file}"
            "$<TARGET_FILE_DIR:QllamaTalkApp>"
            COMMENT "Copying libggml*.so to QllamaTalkApp"
        )
    endforeach()
endif()

# ----------------------------------------------------------------------------
# QMLモジュールの設定 (Configure the QML module)
# ----------------------------------------------------------------------------
qt6_add_qml_module(content
    URI "content"
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        App.qml
        Screen01.ui.qml
        ChatView.qml
        ChatInputField.qml
        Expander.qml
    RESOURCES
        fonts/fonts.txt
        icons/online.svg
        icons/offline.svg
        icons/local_error.svg
        icons/remote_error.svg
        icons/arrow.svg
    SOURCES
        ChatMessageModel.cpp
        ChatMessageModel.h
        LlamaChatEngine.h
        LlamaChatEngine.cpp
        LlamaResponseGenerator.h
        LlamaResponseGenerator.cpp
        RemoteResponseGeneratorCompositor.h
        RemoteResponseGeneratorCompositor.cpp
        QtWebSocketsRemoteGenerator.h
        QtWebSocketsRemoteGenerator.cpp
        QtRemoteObjectsRemoteGenerator.h
        QtRemoteObjectsRemoteGenerator.cpp
        RemoteGeneratorInterface.h
        VoiceRecognitionEngine.h
        VoiceRecognitionEngine.cpp
        VoiceDetector.h
        VoiceDetector.cpp
        common.h
        common.cpp
        dr_wav.h
)

# ----------------------------------------------------------------------------
# 上で集めたライブラリをリンク (Link the collected libraries)
# ----------------------------------------------------------------------------
target_link_libraries(content PRIVATE
    ${ALL_LIBS}
    Qt6::Concurrent
    Qt6::RemoteObjects
    Qt6::WebSockets
    Qt6::Multimedia
)

if(NOT ANDROID)
    # ----------------------------------------------------------------------------
    # ダウンロードした gguf モデルファイルをアプリ実行ファイルの横にコピー
    # Copy the downloaded gguf model file next to the app executable
    # ----------------------------------------------------------------------------
    add_custom_command(
        TARGET content POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_LIST_DIR}/llama_models/${LLAMA_MODEL_NAME}"
            "$<TARGET_FILE_DIR:QllamaTalkApp>"
        COMMENT "Copying ${LLAMA_MODEL_NAME} next to the QllamaTalkApp binary"
    )
endif()
