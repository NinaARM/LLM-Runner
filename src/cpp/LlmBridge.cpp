//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include <iostream>
#include <cstring>
#include <cmath>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <memory>
#include "LlmBridge.hpp"

/**
 * @brief Maps an operation ID to its corresponding WorkState.
 *
 * @details
 * This registry enables lookup and lifecycle management of ongoing work items
 * (e.g., cancel or mark as error) using their operation IDs.
 */
std::unordered_map<long, std::shared_ptr<WorkState>> g_workMap;

// This is the mutex to protect g_workMap
std::mutex g_workMutex;

/**
 * @brief Register or update a work item.
 *
 * Stores the given WorkState in the registry using its @c operationId as the key.
 * If an entry with the same ID already exists, it is replaced.
 *
 * @param state Shared pointer to the WorkState to store. Its @c operationId is used as the key.
 */
void addWork(const std::shared_ptr<WorkState>& state) {
    std::lock_guard<std::mutex> lk(g_workMutex);
    g_workMap[state->operationId] = state;
}

/**
 * @brief Look up a work item by its operation ID.
 *
 * @param operationId The identifier of the work item to find.
 * @return A shared pointer to the WorkState if found; otherwise @c nullptr.
 */
std::shared_ptr<WorkState> findWork(long operationId) {
    std::lock_guard<std::mutex> lk(g_workMutex);
    auto it = g_workMap.find(operationId);
    return (it == g_workMap.end()) ? nullptr : it->second;
}

/**
 * @brief Remove a work item by its operation ID.
 *
 * If an entry exists for the given ID, it is erased from the registry and the
 * corresponding WorkState is returned.
 *
 * @param operationId The identifier of the work item to remove.
 * @return The removed WorkState if present; otherwise @c nullptr.
 */
std::shared_ptr<WorkState> removeWork(long operationId) {
    std::shared_ptr<WorkState> result;

    {
        std::lock_guard<std::mutex> lk(g_workMutex);
        auto it = g_workMap.find(operationId);
        if (it != g_workMap.end()) {
            result = std::move(it->second);
            g_workMap.erase(it);
        }
    }

    return result;
}
