#
# SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# Pull in LLM framework library
if (${LLM_FRAMEWORK} STREQUAL "llama.cpp")
    add_subdirectory(llama_cpp)
elseif(${LLM_FRAMEWORK} STREQUAL "onnxruntime-genai")
    add_subdirectory(onnxruntime_genai)
else()
    message(FATAL_ERROR "${LLM_FRAMEWORK} is currently not supported :(")
endif()
