#
# llama_setup.cmake
#
# This file sets up the llama.cpp submodule, checks out the specified commit,
# configures GPU backends based on OS, builds the library, and installs it
# into a local folder so that it can be linked by the main project.
#
# We try to ensure this setup is done only once (or only when needed)
# in order to avoid repeated builds on every CMake run.
#

# ----------------------------------------------------------------------------
# 1) Define the path for the llama.cpp submodule
# ----------------------------------------------------------------------------
set(LLAMA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/llama.cpp")

# ----------------------------------------------------------------------------
# 2) Update and checkout the correct commit of llama.cpp submodule if needed
#    (if you want to skip this automatically, you can remove or comment-out
#    the following execute_process commands.)
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

# We only check out the specific commit if it's not already at that commit
# (This is a simplistic approach - you can refine it if needed.)
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
# 3) Decide GPU backends based on OS (example approach).
#    You can modify the flags to match llama.cpp's CMake variables if needed.
# ----------------------------------------------------------------------------
set(LLAMA_BUILD_DIR "${LLAMA_SOURCE_DIR}/build")
if(APPLE)
    message(STATUS "llama_setup: Building on Apple -> enable Metal backend")
    set(LLAMA_METAL ON)
elseif(WIN32)
    message(STATUS "llama_setup: Building on Windows -> enable cuBLAS backend")
    set(LLAMA_CUBLAS ON)
elseif(UNIX)
    message(STATUS "llama_setup: Building on UNIX -> enable OpenCL or other backends")
    set(LLAMA_OPENCL ON)
endif()

# ----------------------------------------------------------------------------
# 4) Build llama.cpp only once if not installed. We'll check if a file or dir
#    marking a successful build is present. (You can refine the logic as needed.)
# ----------------------------------------------------------------------------
set(LLAMA_INSTALL_DIR "${LLAMA_BUILD_DIR}/install")
set(LLAMA_BUILD_STAMP "${LLAMA_BUILD_DIR}/.llama_build_done")

if(NOT EXISTS "${LLAMA_BUILD_STAMP}")
    message(STATUS "llama_setup: Llama library not found in build dir -> configuring & building")

    # Make a build directory if not existing
    file(MAKE_DIRECTORY "${LLAMA_BUILD_DIR}")

    # We can use an ExternalProject or just do a direct CMake call:
    # Here is a minimal approach using execute_process for demonstration:
    #  - Adjust the cmake flags to your preference
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
                -B "${LLAMA_BUILD_DIR}"
                -S "${LLAMA_SOURCE_DIR}"
                -DLLAMA_BUILD_METAL=${LLAMA_METAL}
                -DLLAMA_BUILD_CUBLAS=${LLAMA_CUBLAS}
                -DLLAMA_BUILD_OPENCL=${LLAMA_OPENCL}
                -DCMAKE_INSTALL_PREFIX="${LLAMA_INSTALL_DIR}"
        WORKING_DIRECTORY "${LLAMA_BUILD_DIR}"
    )
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${LLAMA_BUILD_DIR}" --target install
        WORKING_DIRECTORY "${LLAMA_BUILD_DIR}"
    )

    # Mark stamp file to skip repeated builds
    file(WRITE "${LLAMA_BUILD_STAMP}" "Llama build success at ${CMAKE_SYSTEM_NAME}")
else()
    message(STATUS "llama_setup: Llama library is already built -> skipping rebuild")
endif()

# ----------------------------------------------------------------------------
# 5) Provide a variable for the llama library path so main project can link it
# ----------------------------------------------------------------------------
set(LLAMA_LIB_DIR "${LLAMA_INSTALL_DIR}/lib" PARENT_SCOPE)
set(LLAMA_INCLUDE_DIR "${LLAMA_INSTALL_DIR}/include" PARENT_SCOPE)
