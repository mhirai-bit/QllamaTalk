#
# llmRemoteServer_setup.cmake
#
# LLMRemoteServer サブモジュールを「最新コミット」に更新する。
#

# 1) サブモジュールのパス
set(LLM_RS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/LLMRemoteServer")

# 2) サブモジュールが初期化されていない or 最新化したい場合、更新を実行
#    --remote オプションを使って "リモート追跡ブランチ" の最新をチェックアウト
message(STATUS "[llmRemoteServer_setup] Ensuring submodule 'LLMRemoteServer' is at latest remote commit...")
execute_process(
    COMMAND git submodule update --init --checkout --recursive --remote LLMRemoteServer
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE LLM_RS_UPDATE_RESULT
)

if(NOT LLM_RS_UPDATE_RESULT EQUAL 0)
    message(FATAL_ERROR "[llmRemoteServer_setup] Failed to update submodule 'LLMRemoteServer' to latest commit")
endif()

message(STATUS "[llmRemoteServer_setup] LLMRemoteServer submodule is now at the latest remote commit.")
