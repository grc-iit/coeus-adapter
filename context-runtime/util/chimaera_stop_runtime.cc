/*
 * Copyright (c) 2024, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 *
 * This file is part of IOWarp Core.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Chimaera runtime shutdown utility
 *
 * Connects to the running Chimaera runtime and sends a StopRuntimeTask
 * via the admin ChiMod client to initiate graceful shutdown.
 */

#include <chrono>
#include <iostream>
#include <thread>

#include "chimaera/admin/admin_client.h"
#include "chimaera/chimaera.h"
#include "chimaera/pool_query.h"
#include "chimaera/types.h"

int main(int argc, char* argv[]) {
  HLOG(kDebug, "Stopping Chimaera runtime...");

  try {
    // Initialize Chimaera client components
    HLOG(kDebug, "Initializing Chimaera client...");
    if (!chi::CHIMAERA_INIT(chi::ChimaeraMode::kClient, false)) {
      HLOG(kError, "Failed to initialize Chimaera client components");
      return 1;
    }

    HLOG(kDebug, "Creating admin client connection...");
    // Create admin client connected to admin pool
    chimaera::admin::Client admin_client(chi::kAdminPoolId);

    // Check if IPC manager is available
    auto* ipc_manager = CHI_IPC;
    if (!ipc_manager || !ipc_manager->IsInitialized()) {
      HLOG(kError, "IPC manager not available - is Chimaera runtime running?");
      return 1;
    }

    // Additional validation: check if TaskQueue is accessible
    auto* task_queue = ipc_manager->GetTaskQueue();
    if (!task_queue) {
      HLOG(kError, "TaskQueue not available - runtime may not be properly initialized");
      return 1;
    }

    // Validate that task queue has valid lane configuration
    try {
      chi::u32 num_lanes = task_queue->GetNumLanes();
      if (num_lanes == 0) {
        HLOG(kError, "TaskQueue has no lanes configured - runtime initialization incomplete");
        return 1;
      }
      HLOG(kDebug, "TaskQueue validated with {} lanes", num_lanes);
    } catch (const std::exception& e) {
      HLOG(kError, "TaskQueue validation failed: {}", e.what());
      return 1;
    }

    // Create domain query for local execution
    chi::PoolQuery pool_query;

    // Parse command line arguments for shutdown parameters
    chi::u32 shutdown_flags = 0;
    chi::u32 grace_period_ms = 5000;  // Default 5 seconds

    if (argc >= 2) {
      grace_period_ms = static_cast<chi::u32>(std::atoi(argv[1]));
      if (grace_period_ms == 0) {
        grace_period_ms = 5000;  // Fallback to default
      }
    }

    HLOG(kDebug, "Sending stop runtime task to admin pool (grace period: {}ms)...", grace_period_ms);

    // Send StopRuntimeTask via admin client
    HLOG(kDebug, "Calling admin client AsyncStopRuntime...");
    auto start_time = std::chrono::steady_clock::now();

    // Use the admin client's AsyncStopRuntime method - fire and forget
    chi::Future<chimaera::admin::StopRuntimeTask> stop_task;
    try {
      stop_task = admin_client.AsyncStopRuntime(pool_query, shutdown_flags, grace_period_ms);
      if (stop_task.IsNull()) {
        HLOG(kError, "Failed to create stop runtime task - runtime may not be running");
        return 1;
      }
    } catch (const std::exception& e) {
      HLOG(kError, "Error creating stop runtime task: {}", e.what());
      return 1;
    }

    HLOG(kDebug, "Stop runtime task submitted successfully (fire-and-forget)");

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        end_time - start_time)
                        .count();

    HLOG(kDebug, "Runtime stop task submitted in {}ms", duration);

    return 0;

  } catch (const std::exception& e) {
    HLOG(kError, "Error stopping runtime: {}", e.what());
    return 1;
  }
}