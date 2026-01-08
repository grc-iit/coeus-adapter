/**
 * Chimaera runtime startup utility
 */

#include <chrono>
#include <iostream>
#include <thread>

#include "chimaera/chimaera.h"
#include "chimaera/singletons.h"
#include "chimaera/types.h"

namespace {
volatile bool g_keep_running = true;

/**
 * Find and initialize the admin ChiMod
 * Creates a ChiPool for the admin module using PoolManager
 * @return true if successful, false on failure
 */
bool InitializeAdminChiMod() {
  HLOG(kDebug, "Initializing admin ChiMod...");

  // Get the module manager to find the admin chimod
  auto* module_manager = CHI_MODULE_MANAGER;
  if (!module_manager) {
    HLOG(kError, "Module manager not available");
    return false;
  }

  // Check if admin chimod is available
  auto* admin_chimod = module_manager->GetChiMod("chimaera_admin");
  if (!admin_chimod) {
    HLOG(kError, "CRITICAL: Admin ChiMod not found! This is a required system component.");
    return false;
  }

  // Get the pool manager to register the admin pool
  auto* pool_manager = CHI_POOL_MANAGER;
  if (!pool_manager) {
    HLOG(kError, "Pool manager not available");
    return false;
  }

  try {
    // Use PoolManager to create the admin pool
    // This functionality is now handled by PoolManager::ServerInit()
    // which calls CreatePool internally with proper task and RunContext
    // No need to manually create admin pool here anymore
    HLOG(kDebug, "Admin pool creation handled by PoolManager::ServerInit()");

    // Verify the pool was created successfully
    if (!pool_manager->HasPool(chi::kAdminPoolId)) {
      HLOG(kError, "Admin pool creation reported success but pool is not found");
      return false;
    }

    HLOG(kDebug, "Admin ChiPool created successfully (ID: {})", chi::kAdminPoolId);
    return true;

  } catch (const std::exception& e) {
    HLOG(kError, "Exception during admin ChiMod initialization: {}", e.what());
    return false;
  }
}

/**
 * Shutdown the admin ChiMod properly
 */
void ShutdownAdminChiMod() {
  HLOG(kDebug, "Shutting down admin ChiMod...");

  try {
    // Get the pool manager to destroy the admin pool
    auto* pool_manager = CHI_POOL_MANAGER;
    if (pool_manager && pool_manager->HasPool(chi::kAdminPoolId)) {
      // Use PoolManager to destroy the admin pool locally
      if (pool_manager->DestroyLocalPool(chi::kAdminPoolId)) {
        HLOG(kDebug, "Admin pool destroyed successfully");
      } else {
        HLOG(kError, "Failed to destroy admin pool");
      }
    }

  } catch (const std::exception& e) {
    HLOG(kError, "Exception during admin ChiMod shutdown: {}", e.what());
  }

  HLOG(kDebug, "Admin ChiMod shutdown complete");
}

}  // namespace

int main(int argc, char* argv[]) {
  HLOG(kDebug, "Starting Chimaera runtime...");

  // Initialize Chimaera runtime
  if (!chi::CHIMAERA_INIT(chi::ChimaeraMode::kRuntime, true)) {
    HLOG(kError, "Failed to initialize Chimaera runtime");
    return 1;
  }

  HLOG(kDebug, "Chimaera runtime started successfully");

  // Find and initialize admin ChiMod
  if (!InitializeAdminChiMod()) {
    HLOG(kError, "FATAL ERROR: Failed to find or initialize admin ChiMod");
    return 1;
  }

  HLOG(kDebug, "Admin ChiMod initialized successfully with pool ID {}", chi::kAdminPoolId);

  // Main runtime loop
  while (g_keep_running) {
    // Sleep for a short period
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  HLOG(kDebug, "Shutting down Chimaera runtime...");

  // Shutdown admin pool first
  ShutdownAdminChiMod();

  HLOG(kDebug, "Chimaera runtime stopped (finalization will happen automatically)");
  return 0;
}