# HSHM Dynamic Libraries Guide

## Overview

The Dynamic Libraries API in Hermes Shared Memory (HSHM) provides cross-platform functionality for loading shared libraries at runtime, enabling plugin architectures and modular application design. This guide covers the `SharedLibrary` class and related patterns for dynamic library management.

## SharedLibrary Class

### Basic Library Loading

```cpp
#include "hermes_shm/introspect/system_info.h"

// Load a shared library
hshm::SharedLibrary math_lib("./libmymath.so");      // Linux
// hshm::SharedLibrary math_lib("libmymath.dylib");   // macOS
// hshm::SharedLibrary math_lib("mymath.dll");        // Windows

// Check if loading succeeded
if (!math_lib.IsNull()) {
    printf("Library loaded successfully\n");
} else {
    printf("Failed to load library: %s\n", math_lib.GetError().c_str());
}

// Load library with full path
hshm::SharedLibrary lib("/usr/local/lib/libcustom.so");

// Delayed loading
hshm::SharedLibrary delayed_lib;
// ... some time later ...
delayed_lib.Load("./plugins/myplugin.so");
```

### Getting Symbols

```cpp
// Get function pointer
typedef double (*calculate_fn)(double, double);
calculate_fn calculate = (calculate_fn)math_lib.GetSymbol("calculate");

if (calculate != nullptr) {
    double result = calculate(10.0, 20.0);
    printf("Calculation result: %f\n", result);
} else {
    printf("Function 'calculate' not found: %s\n", math_lib.GetError().c_str());
}

// Get global variable
int* library_version = (int*)math_lib.GetSymbol("library_version");
if (library_version != nullptr) {
    printf("Library version: %d\n", *library_version);
    *library_version = 42;  // Modify shared library global
}

// Get struct or class
struct LibraryInfo {
    char name[64];
    int major_version;
    int minor_version;
};

LibraryInfo* info = (LibraryInfo*)math_lib.GetSymbol("library_info");
if (info != nullptr) {
    printf("Library: %s v%d.%d\n", info->name, 
           info->major_version, info->minor_version);
}
```

### Error Handling

```cpp
class SafeLibraryLoader {
public:
    static bool LoadLibraryWithFallback(
        hshm::SharedLibrary& lib,
        const std::vector<std::string>& paths) {
        
        for (const auto& path : paths) {
            lib.Load(path);
            if (!lib.IsNull()) {
                printf("Loaded library from: %s\n", path.c_str());
                return true;
            }
            printf("Failed to load %s: %s\n", path.c_str(), lib.GetError().c_str());
        }
        
        return false;
    }
    
    static void* GetRequiredSymbol(
        hshm::SharedLibrary& lib,
        const std::string& symbol_name) {
        
        void* symbol = lib.GetSymbol(symbol_name);
        if (symbol == nullptr) {
            throw std::runtime_error(
                "Required symbol '" + symbol_name + "' not found: " + lib.GetError()
            );
        }
        return symbol;
    }
};

// Usage
hshm::SharedLibrary my_lib;
std::vector<std::string> search_paths = {
    "./libmylib.so",
    "/usr/local/lib/libmylib.so",
    "/usr/lib/libmylib.so"
};

if (SafeLibraryLoader::LoadLibraryWithFallback(my_lib, search_paths)) {
    try {
        auto init_fn = (void(*)())SafeLibraryLoader::GetRequiredSymbol(my_lib, "initialize");
        init_fn();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
```

## Plugin Architecture

### Plugin Interface Definition

```cpp
// plugin_interface.h - Shared between application and plugins
#pragma once

class IPlugin {
public:
    virtual ~IPlugin() = default;
    
    // Plugin identification
    virtual const char* GetName() const = 0;
    virtual const char* GetVersion() const = 0;
    virtual const char* GetDescription() const = 0;
    
    // Lifecycle
    virtual bool Initialize(void* context) = 0;
    virtual void Execute() = 0;
    virtual void Shutdown() = 0;
    
    // Optional capabilities
    virtual bool SupportsFeature(const char* feature) const { return false; }
    virtual void* GetInterface(const char* interface_name) { return nullptr; }
};

// Plugin factory function types
typedef IPlugin* (*CreatePluginFunc)();
typedef void (*DestroyPluginFunc)(IPlugin*);
typedef const char* (*GetPluginAPIVersionFunc)();

// Current plugin API version
#define PLUGIN_API_VERSION "1.0.0"
```

### Plugin Manager Implementation

```cpp
class PluginManager {
public:
    struct PluginInfo {
        std::string path;
        std::string name;
        std::string version;
        std::string description;
        bool enabled;
    };
    
private:
    struct LoadedPlugin {
        hshm::SharedLibrary library;
        IPlugin* instance;
        DestroyPluginFunc destroy_func;
        PluginInfo info;
        
        LoadedPlugin(hshm::SharedLibrary&& lib, IPlugin* inst, 
                    DestroyPluginFunc destroy, const PluginInfo& info)
            : library(std::move(lib)), instance(inst), 
              destroy_func(destroy), info(info) {}
    };
    
    std::vector<std::unique_ptr<LoadedPlugin>> plugins_;
    std::map<std::string, size_t> plugin_index_;  // name -> index mapping
    void* app_context_;
    
public:
    explicit PluginManager(void* context = nullptr) : app_context_(context) {}
    
    bool LoadPlugin(const std::string& plugin_path) {
        printf("Loading plugin: %s\n", plugin_path.c_str());
        
        // Check if already loaded
        if (IsPluginLoaded(plugin_path)) {
            printf("Plugin already loaded: %s\n", plugin_path.c_str());
            return true;
        }
        
        // Load the library
        hshm::SharedLibrary lib(plugin_path);
        if (lib.IsNull()) {
            fprintf(stderr, "Failed to load plugin library: %s\n", 
                    lib.GetError().c_str());
            return false;
        }
        
        // Check API version
        if (!CheckAPIVersion(lib)) {
            fprintf(stderr, "Plugin API version mismatch\n");
            return false;
        }
        
        // Get factory functions
        CreatePluginFunc create = (CreatePluginFunc)lib.GetSymbol("CreatePlugin");
        DestroyPluginFunc destroy = (DestroyPluginFunc)lib.GetSymbol("DestroyPlugin");
        
        if (!create || !destroy) {
            fprintf(stderr, "Plugin missing required factory functions\n");
            return false;
        }
        
        // Create plugin instance
        IPlugin* plugin = create();
        if (!plugin) {
            fprintf(stderr, "Failed to create plugin instance\n");
            return false;
        }
        
        // Get plugin information
        PluginInfo info;
        info.path = plugin_path;
        info.name = plugin->GetName();
        info.version = plugin->GetVersion();
        info.description = plugin->GetDescription();
        info.enabled = false;
        
        // Initialize plugin
        if (!plugin->Initialize(app_context_)) {
            fprintf(stderr, "Plugin initialization failed: %s\n", info.name.c_str());
            destroy(plugin);
            return false;
        }
        
        info.enabled = true;
        printf("Plugin loaded successfully: %s v%s\n", 
               info.name.c_str(), info.version.c_str());
        printf("  Description: %s\n", info.description.c_str());
        
        // Store plugin
        size_t index = plugins_.size();
        plugin_index_[info.name] = index;
        plugins_.emplace_back(std::make_unique<LoadedPlugin>(
            std::move(lib), plugin, destroy, info));
        
        return true;
    }
    
    void LoadAllPlugins(const std::string& plugin_dir) {
        printf("Scanning for plugins in: %s\n", plugin_dir.c_str());
        
        std::vector<std::string> plugin_files = ScanPluginDirectory(plugin_dir);
        
        for (const auto& file : plugin_files) {
            LoadPlugin(file);
        }
        
        printf("Loaded %zu plugins\n", plugins_.size());
    }
    
    void ExecutePlugin(const std::string& plugin_name) {
        auto it = plugin_index_.find(plugin_name);
        if (it != plugin_index_.end()) {
            auto& plugin = plugins_[it->second];
            if (plugin->info.enabled) {
                printf("Executing plugin: %s\n", plugin_name.c_str());
                plugin->instance->Execute();
            } else {
                printf("Plugin %s is disabled\n", plugin_name.c_str());
            }
        } else {
            printf("Plugin not found: %s\n", plugin_name.c_str());
        }
    }
    
    void ExecuteAllPlugins() {
        for (auto& loaded : plugins_) {
            if (loaded->info.enabled) {
                printf("Executing plugin: %s\n", loaded->info.name.c_str());
                loaded->instance->Execute();
            }
        }
    }
    
    void DisablePlugin(const std::string& plugin_name) {
        auto it = plugin_index_.find(plugin_name);
        if (it != plugin_index_.end()) {
            plugins_[it->second]->info.enabled = false;
            printf("Plugin disabled: %s\n", plugin_name.c_str());
        }
    }
    
    void EnablePlugin(const std::string& plugin_name) {
        auto it = plugin_index_.find(plugin_name);
        if (it != plugin_index_.end()) {
            plugins_[it->second]->info.enabled = true;
            printf("Plugin enabled: %s\n", plugin_name.c_str());
        }
    }
    
    std::vector<PluginInfo> GetPluginList() const {
        std::vector<PluginInfo> list;
        for (const auto& loaded : plugins_) {
            list.push_back(loaded->info);
        }
        return list;
    }
    
    IPlugin* GetPlugin(const std::string& plugin_name) {
        auto it = plugin_index_.find(plugin_name);
        if (it != plugin_index_.end()) {
            return plugins_[it->second]->instance;
        }
        return nullptr;
    }
    
    ~PluginManager() {
        // Clean shutdown of all plugins
        for (auto& loaded : plugins_) {
            printf("Shutting down plugin: %s\n", loaded->info.name.c_str());
            loaded->instance->Shutdown();
            loaded->destroy_func(loaded->instance);
        }
    }
    
private:
    bool CheckAPIVersion(hshm::SharedLibrary& lib) {
        GetPluginAPIVersionFunc get_version = 
            (GetPluginAPIVersionFunc)lib.GetSymbol("GetPluginAPIVersion");
        
        if (get_version) {
            const char* version = get_version();
            if (strcmp(version, PLUGIN_API_VERSION) != 0) {
                fprintf(stderr, "API version mismatch: expected %s, got %s\n",
                        PLUGIN_API_VERSION, version);
                return false;
            }
        }
        return true;
    }
    
    bool IsPluginLoaded(const std::string& path) {
        for (const auto& loaded : plugins_) {
            if (loaded->info.path == path) {
                return true;
            }
        }
        return false;
    }
    
    std::vector<std::string> ScanPluginDirectory(const std::string& dir) {
        std::vector<std::string> plugin_files;
        
#ifdef __linux__
        DIR* d = opendir(dir.c_str());
        if (d) {
            struct dirent* entry;
            while ((entry = readdir(d)) != nullptr) {
                std::string filename = entry->d_name;
                if (filename.find(".so") != std::string::npos) {
                    plugin_files.push_back(dir + "/" + filename);
                }
            }
            closedir(d);
        }
#elif __APPLE__
        // Scan for .dylib files on macOS
        DIR* d = opendir(dir.c_str());
        if (d) {
            struct dirent* entry;
            while ((entry = readdir(d)) != nullptr) {
                std::string filename = entry->d_name;
                if (filename.find(".dylib") != std::string::npos) {
                    plugin_files.push_back(dir + "/" + filename);
                }
            }
            closedir(d);
        }
#elif _WIN32
        // Scan for .dll files on Windows
        std::string pattern = dir + "\\*.dll";
        WIN32_FIND_DATA fd;
        HANDLE hFind = FindFirstFile(pattern.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                plugin_files.push_back(dir + "\\" + fd.cFileName);
            } while (FindNextFile(hFind, &fd));
            FindClose(hFind);
        }
#endif
        
        return plugin_files;
    }
};
```

### Example Plugin Implementation

```cpp
// myplugin.cpp - Compile as shared library
#include "plugin_interface.h"
#include <cstring>

class MyPlugin : public IPlugin {
    std::string name_ = "MyPlugin";
    std::string version_ = "1.0.0";
    std::string description_ = "Example plugin implementation";
    void* app_context_;
    
public:
    const char* GetName() const override { 
        return name_.c_str(); 
    }
    
    const char* GetVersion() const override { 
        return version_.c_str(); 
    }
    
    const char* GetDescription() const override { 
        return description_.c_str(); 
    }
    
    bool Initialize(void* context) override {
        printf("MyPlugin: Initializing...\n");
        app_context_ = context;
        
        // Perform initialization
        if (!LoadConfiguration()) {
            return false;
        }
        
        if (!AllocateResources()) {
            return false;
        }
        
        printf("MyPlugin: Initialization complete\n");
        return true;
    }
    
    void Execute() override {
        printf("MyPlugin: Executing main functionality\n");
        
        // Perform plugin work
        ProcessData();
        GenerateOutput();
    }
    
    void Shutdown() override {
        printf("MyPlugin: Cleaning up resources\n");
        
        // Clean up resources
        FreeResources();
    }
    
    bool SupportsFeature(const char* feature) const override {
        // Check for specific features
        if (strcmp(feature, "data_processing") == 0) return true;
        if (strcmp(feature, "report_generation") == 0) return true;
        return false;
    }
    
    void* GetInterface(const char* interface_name) override {
        // Return specialized interfaces
        if (strcmp(interface_name, "IDataProcessor") == 0) {
            return static_cast<IDataProcessor*>(this);
        }
        return nullptr;
    }
    
private:
    bool LoadConfiguration() {
        // Load plugin-specific configuration
        return true;
    }
    
    bool AllocateResources() {
        // Allocate necessary resources
        return true;
    }
    
    void FreeResources() {
        // Free allocated resources
    }
    
    void ProcessData() {
        // Main processing logic
    }
    
    void GenerateOutput() {
        // Generate output/reports
    }
};

// Factory functions (must be extern "C" to prevent name mangling)
extern "C" {
    IPlugin* CreatePlugin() {
        return new MyPlugin();
    }
    
    void DestroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
    
    const char* GetPluginAPIVersion() {
        return PLUGIN_API_VERSION;
    }
}
```

## Cross-Platform Library Loading

### Platform-Agnostic Loader

```cpp
class CrossPlatformLoader {
public:
    static std::string GetLibraryExtension() {
#ifdef _WIN32
        return ".dll";
#elif __APPLE__
        return ".dylib";
#else
        return ".so";
#endif
    }
    
    static std::string GetLibraryPrefix() {
#ifdef _WIN32
        return "";  // No prefix on Windows
#else
        return "lib";  // Unix convention
#endif
    }
    
    static std::string MakeLibraryName(const std::string& base_name) {
        return GetLibraryPrefix() + base_name + GetLibraryExtension();
    }
    
    static std::string GetSystemLibraryPath() {
#ifdef _WIN32
        return "C:\\Windows\\System32";
#elif __APPLE__
        return "/usr/lib:/usr/local/lib";
#else
        return "/usr/lib:/usr/local/lib:/lib";
#endif
    }
    
    static bool LoadLibrary(const std::string& base_name, 
                          hshm::SharedLibrary& lib) {
        // Build search paths
        std::vector<std::string> search_paths = BuildSearchPaths(base_name);
        
        // Try to load from each path
        for (const auto& path : search_paths) {
            lib.Load(path);
            
            if (!lib.IsNull()) {
                printf("Loaded library from: %s\n", path.c_str());
                return true;
            }
        }
        
        fprintf(stderr, "Failed to find library: %s\n", base_name.c_str());
        return false;
    }
    
private:
    static std::vector<std::string> BuildSearchPaths(const std::string& base_name) {
        std::vector<std::string> paths;
        std::string lib_name = MakeLibraryName(base_name);
        
        // Current directory
        paths.push_back("./" + lib_name);
        
        // Application library directory
        std::string app_lib = hshm::SystemInfo::Getenv("APP_LIB_DIR");
        if (!app_lib.empty()) {
            paths.push_back(app_lib + "/" + lib_name);
        }
        
        // LD_LIBRARY_PATH / DYLD_LIBRARY_PATH / PATH
#ifdef _WIN32
        std::string env_path = hshm::SystemInfo::Getenv("PATH");
#elif __APPLE__
        std::string env_path = hshm::SystemInfo::Getenv("DYLD_LIBRARY_PATH");
#else
        std::string env_path = hshm::SystemInfo::Getenv("LD_LIBRARY_PATH");
#endif
        
        if (!env_path.empty()) {
            AddPathsFromEnvironment(env_path, lib_name, paths);
        }
        
        // System paths
        AddSystemPaths(lib_name, paths);
        
        return paths;
    }
    
    static void AddPathsFromEnvironment(const std::string& env_path,
                                       const std::string& lib_name,
                                       std::vector<std::string>& paths) {
        std::stringstream ss(env_path);
        std::string path;
        
#ifdef _WIN32
        const char delimiter = ';';
#else
        const char delimiter = ':';
#endif
        
        while (std::getline(ss, path, delimiter)) {
            if (!path.empty()) {
                paths.push_back(path + "/" + lib_name);
            }
        }
    }
    
    static void AddSystemPaths(const std::string& lib_name,
                              std::vector<std::string>& paths) {
#ifdef _WIN32
        paths.push_back("C:\\Windows\\System32\\" + lib_name);
        paths.push_back("C:\\Windows\\SysWOW64\\" + lib_name);
#elif __APPLE__
        paths.push_back("/usr/local/lib/" + lib_name);
        paths.push_back("/usr/lib/" + lib_name);
        paths.push_back("/opt/homebrew/lib/" + lib_name);  // Apple Silicon
#else
        paths.push_back("/usr/local/lib/" + lib_name);
        paths.push_back("/usr/lib/" + lib_name);
        paths.push_back("/lib/" + lib_name);
        paths.push_back("/usr/lib/x86_64-linux-gnu/" + lib_name);  // Debian/Ubuntu
#endif
    }
};
```

### Version-Aware Loading

```cpp
class VersionedLibraryLoader {
public:
    struct Version {
        int major;
        int minor;
        int patch;
        
        std::string ToString() const {
            return std::to_string(major) + "." + 
                   std::to_string(minor) + "." + 
                   std::to_string(patch);
        }
    };
    
    static bool LoadVersionedLibrary(const std::string& base_name,
                                    const Version& min_version,
                                    hshm::SharedLibrary& lib) {
        // Try exact version first
        std::string versioned_name = base_name + "-" + min_version.ToString();
        if (CrossPlatformLoader::LoadLibrary(versioned_name, lib)) {
            if (CheckVersion(lib, min_version)) {
                return true;
            }
        }
        
        // Try major.minor version
        versioned_name = base_name + "-" + 
                        std::to_string(min_version.major) + "." + 
                        std::to_string(min_version.minor);
        if (CrossPlatformLoader::LoadLibrary(versioned_name, lib)) {
            if (CheckVersion(lib, min_version)) {
                return true;
            }
        }
        
        // Try major version only
        versioned_name = base_name + "-" + std::to_string(min_version.major);
        if (CrossPlatformLoader::LoadLibrary(versioned_name, lib)) {
            if (CheckVersion(lib, min_version)) {
                return true;
            }
        }
        
        // Try unversioned
        if (CrossPlatformLoader::LoadLibrary(base_name, lib)) {
            if (CheckVersion(lib, min_version)) {
                return true;
            }
        }
        
        return false;
    }
    
private:
    static bool CheckVersion(hshm::SharedLibrary& lib, const Version& min_version) {
        typedef void (*GetVersionFunc)(int*, int*, int*);
        GetVersionFunc get_version = (GetVersionFunc)lib.GetSymbol("GetLibraryVersion");
        
        if (get_version) {
            Version lib_version;
            get_version(&lib_version.major, &lib_version.minor, &lib_version.patch);
            
            if (lib_version.major > min_version.major) return true;
            if (lib_version.major < min_version.major) return false;
            
            if (lib_version.minor > min_version.minor) return true;
            if (lib_version.minor < min_version.minor) return false;
            
            return lib_version.patch >= min_version.patch;
        }
        
        // No version function, assume compatible
        return true;
    }
};
```

## Advanced Plugin Features

### Hot-Reloading Plugins

```cpp
class HotReloadablePluginManager : public PluginManager {
    std::map<std::string, std::time_t> plugin_timestamps_;
    std::thread monitor_thread_;
    std::atomic<bool> monitoring_;
    
public:
    void StartHotReload(int check_interval_seconds = 5) {
        monitoring_ = true;
        monitor_thread_ = std::thread([this, check_interval_seconds]() {
            MonitorPlugins(check_interval_seconds);
        });
    }
    
    void StopHotReload() {
        monitoring_ = false;
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
    }
    
private:
    void MonitorPlugins(int interval) {
        while (monitoring_) {
            CheckForUpdates();
            std::this_thread::sleep_for(std::chrono::seconds(interval));
        }
    }
    
    void CheckForUpdates() {
        auto plugin_list = GetPluginList();
        
        for (const auto& info : plugin_list) {
            struct stat st;
            if (stat(info.path.c_str(), &st) == 0) {
                auto it = plugin_timestamps_.find(info.path);
                if (it != plugin_timestamps_.end()) {
                    if (st.st_mtime > it->second) {
                        printf("Plugin %s has been updated, reloading...\n", 
                               info.name.c_str());
                        ReloadPlugin(info.name);
                        plugin_timestamps_[info.path] = st.st_mtime;
                    }
                } else {
                    plugin_timestamps_[info.path] = st.st_mtime;
                }
            }
        }
    }
    
    void ReloadPlugin(const std::string& plugin_name) {
        // Find and unload the plugin
        auto it = plugin_index_.find(plugin_name);
        if (it != plugin_index_.end()) {
            auto& plugin = plugins_[it->second];
            std::string path = plugin->info.path;
            
            // Shutdown and destroy
            plugin->instance->Shutdown();
            plugin->destroy_func(plugin->instance);
            
            // Remove from list
            plugins_.erase(plugins_.begin() + it->second);
            plugin_index_.erase(it);
            
            // Reload
            LoadPlugin(path);
        }
    }
};
```

## Complete Example: Extensible Application

```cpp
#include "hermes_shm/introspect/system_info.h"
#include <iostream>
#include <memory>

class ExtensibleApplication {
    std::unique_ptr<PluginManager> plugin_manager_;
    std::string plugin_directory_;
    
public:
    ExtensibleApplication() {
        plugin_manager_ = std::make_unique<PluginManager>(this);
        plugin_directory_ = GetPluginDirectory();
    }
    
    int Run(int argc, char* argv[]) {
        try {
            // Initialize application
            if (!Initialize()) {
                return 1;
            }
            
            // Load plugins
            LoadPlugins();
            
            // Display loaded plugins
            DisplayPlugins();
            
            // Execute plugins
            ExecutePlugins();
            
            // Run main application loop
            return MainLoop();
            
        } catch (const std::exception& e) {
            std::cerr << "Application error: " << e.what() << std::endl;
            return 1;
        }
    }
    
private:
    bool Initialize() {
        printf("Initializing extensible application...\n");
        
        // Set up plugin environment
        SetupPluginEnvironment();
        
        return true;
    }
    
    void SetupPluginEnvironment() {
        // Add plugin directory to library path
        std::string ld_path = hshm::SystemInfo::Getenv("LD_LIBRARY_PATH");
        if (!ld_path.empty()) {
            ld_path = plugin_directory_ + ":" + ld_path;
        } else {
            ld_path = plugin_directory_;
        }
        hshm::SystemInfo::Setenv("LD_LIBRARY_PATH", ld_path, 1);
        
        // Set plugin-specific environment
        hshm::SystemInfo::Setenv("PLUGIN_API_VERSION", PLUGIN_API_VERSION, 1);
        hshm::SystemInfo::Setenv("APP_PLUGIN_DIR", plugin_directory_, 1);
    }
    
    std::string GetPluginDirectory() {
        // Check environment variable
        std::string dir = hshm::SystemInfo::Getenv("APP_PLUGIN_DIR");
        if (!dir.empty()) {
            return dir;
        }
        
        // Check relative to executable
        std::string exe_dir = GetExecutableDirectory();
        if (!exe_dir.empty()) {
            return exe_dir + "/plugins";
        }
        
        // Default
        return "./plugins";
    }
    
    std::string GetExecutableDirectory() {
#ifdef __linux__
        char path[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path)-1);
        if (len != -1) {
            path[len] = '\0';
            std::string exe_path(path);
            return exe_path.substr(0, exe_path.find_last_of('/'));
        }
#endif
        return "";
    }
    
    void LoadPlugins() {
        printf("Loading plugins from: %s\n", plugin_directory_.c_str());
        
        // Load all plugins from directory
        plugin_manager_->LoadAllPlugins(plugin_directory_);
        
        // Load specific required plugins
        LoadRequiredPlugin("core_plugin");
        LoadRequiredPlugin("ui_plugin");
    }
    
    void LoadRequiredPlugin(const std::string& plugin_name) {
        if (!plugin_manager_->GetPlugin(plugin_name)) {
            std::string plugin_file = plugin_directory_ + "/" + 
                CrossPlatformLoader::MakeLibraryName(plugin_name);
            
            if (!plugin_manager_->LoadPlugin(plugin_file)) {
                fprintf(stderr, "Required plugin %s not found\n", plugin_name.c_str());
            }
        }
    }
    
    void DisplayPlugins() {
        auto plugins = plugin_manager_->GetPluginList();
        
        printf("\nLoaded Plugins (%zu):\n", plugins.size());
        printf("%-20s %-10s %-10s %s\n", "Name", "Version", "Status", "Description");
        printf("%-20s %-10s %-10s %s\n", "----", "-------", "------", "-----------");
        
        for (const auto& info : plugins) {
            printf("%-20s %-10s %-10s %s\n",
                   info.name.c_str(),
                   info.version.c_str(),
                   info.enabled ? "Enabled" : "Disabled",
                   info.description.c_str());
        }
        printf("\n");
    }
    
    void ExecutePlugins() {
        printf("Executing all enabled plugins...\n");
        plugin_manager_->ExecuteAllPlugins();
    }
    
    int MainLoop() {
        printf("Application running. Press 'q' to quit.\n");
        
        char command;
        while (std::cin >> command) {
            if (command == 'q') {
                break;
            } else if (command == 'r') {
                // Reload plugins
                LoadPlugins();
                DisplayPlugins();
            } else if (command == 'e') {
                // Execute plugins
                ExecutePlugins();
            } else if (command == 'l') {
                // List plugins
                DisplayPlugins();
            }
        }
        
        printf("Application shutting down...\n");
        return 0;
    }
};

int main(int argc, char* argv[]) {
    ExtensibleApplication app;
    return app.Run(argc, argv);
}
```

## Best Practices

1. **Error Handling**: Always check `IsNull()` and use `GetError()` for diagnostics
2. **Symbol Verification**: Verify function pointers are not null before calling
3. **Name Mangling**: Use `extern "C"` for plugin factory functions to prevent C++ name mangling
4. **RAII Pattern**: Use move semantics and automatic cleanup via destructors
5. **Version Checking**: Implement API version checking for plugin compatibility
6. **Search Paths**: Implement flexible library search paths for deployment flexibility
7. **Platform Abstraction**: Use wrapper functions to handle platform differences
8. **Resource Management**: Ensure plugins properly clean up resources in shutdown
9. **Thread Safety**: Consider thread safety when loading/unloading plugins
10. **Documentation**: Document plugin interfaces thoroughly for third-party developers