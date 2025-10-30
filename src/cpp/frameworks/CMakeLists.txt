#
# SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# We default to share libs on for llama.cpp on android.
# If the user attempts to configure this target with BUILD_SHARED_LIBS OFF
# a fatal error will be thrown in llama_cpp/CMakeLists.txt.
if (${LLM_FRAMEWORK} STREQUAL "llama.cpp" AND DEFINED ANDROID_ABI)
    option(BUILD_SHARED_LIBS "BUILD_SHARED_LIBS" ON)
else()
    option(BUILD_SHARED_LIBS "BUILD_SHARED_LIBS" OFF)
endif()
message (STATUS "BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")

# Pull in LLM framework library
if (${LLM_FRAMEWORK} STREQUAL "llama.cpp")
    add_subdirectory(llama_cpp)
elseif(${LLM_FRAMEWORK} STREQUAL "onnxruntime-genai")
    add_subdirectory(onnxruntime_genai)
elseif(${LLM_FRAMEWORK} STREQUAL "mnn")
    add_subdirectory(mnn)
elseif(${LLM_FRAMEWORK} STREQUAL "mediapipe")
    add_subdirectory(mediapipe)
    message(WARNING "Mediapipe support is currently experimental and limited to models like Gemma-2B, Phi-2 and Falcon-RW-1B")
else()
    message(FATAL_ERROR "${LLM_FRAMEWORK} is currently not supported :(")
endif()
