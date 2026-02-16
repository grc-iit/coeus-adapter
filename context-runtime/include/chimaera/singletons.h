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
 * Central header for Chimaera singleton access macros
 * 
 * This header provides convenient macros for accessing all Chimaera singletons
 * using HSHM's global cross pointer variable pattern. Include this header to
 * get access to all singleton macros in one place.
 */

#ifndef CHIMAERA_INCLUDE_CHIMAERA_SINGLETONS_H_
#define CHIMAERA_INCLUDE_CHIMAERA_SINGLETONS_H_

#include "chimaera/chimaera_manager.h"
#include "chimaera/config_manager.h"
#include "chimaera/ipc_manager.h"
#include "chimaera/pool_manager.h"
#include "chimaera/module_manager.h"
#include "chimaera/work_orchestrator.h"
#include "chimaera/admin.h"

/**
 * Convenience macros for accessing Chimaera singletons
 * 
 * These macros provide easy access to all Chimaera singleton managers
 * using HSHM's global cross pointer variable pattern.
 */

// Core Framework Singleton Access
// CHI_CHIMAERA_MANAGER - Main Chimaera framework coordinator
// CHI_CONFIG_MANAGER   - Configuration manager for YAML parsing
// CHI_IPC              - IPC manager for shared memory and networking
// CHI_POOL_MANAGER     - Pool manager for ChiPools and ChiContainers
// CHI_MODULE_MANAGER   - Module manager for dynamic loading
// CHI_WORK_ORCHESTRATOR - Work orchestrator for thread management
// CHI_ADMIN            - Admin ChiMod client singleton

// All macros are defined in their respective header files:
// - CHI_CHIMAERA_MANAGER defined in chimaera/chimaera_manager.h
// - CHI_CONFIG_MANAGER defined in chimaera/config_manager.h
// - CHI_IPC defined in chimaera/ipc_manager.h
// - CHI_POOL_MANAGER defined in chimaera/pool_manager.h
// - CHI_MODULE_MANAGER defined in chimaera/module_manager.h
// - CHI_WORK_ORCHESTRATOR defined in chimaera/work_orchestrator.h
// - CHI_ADMIN defined in chimaera/admin.h

/**
 * Example usage:
 * 
 * // Initialize the configuration manager
 * CHI_CONFIG_MANAGER->Init();
 *
 * // Get worker thread count from config
 * u32 workers = CHI_CONFIG_MANAGER->GetNumThreads();
 *
 * // Initialize IPC components
 * CHI_IPC->ServerInit();
 * 
 * // Start worker threads
 * CHI_WORK_ORCHESTRATOR->Init();
 * CHI_WORK_ORCHESTRATOR->StartWorkers();
 * 
 * // Register a pool
 * CHI_POOL_MANAGER->RegisterContainer(pool_id, container);
 */

#endif  // CHIMAERA_INCLUDE_CHIMAERA_SINGLETONS_H_