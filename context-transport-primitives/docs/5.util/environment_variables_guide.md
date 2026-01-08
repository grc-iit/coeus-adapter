# HSHM Environment Variables Guide

## Overview

The Environment Variables API in Hermes Shared Memory (HSHM) provides cross-platform functionality for managing environment variables, enabling runtime configuration and dynamic application behavior. This guide covers the `SystemInfo` class methods for environment variable operations.

## Basic Environment Operations

### Getting Environment Variables

```cpp
#include "hermes_shm/introspect/system_info.h"

// Get environment variables with optional size limits
std::string home_dir = hshm::SystemInfo::Getenv("HOME");
std::string path = hshm::SystemInfo::Getenv("PATH", hshm::Unit<size_t>::Kilobytes(64));
std::string user = hshm::SystemInfo::Getenv("USER");

// Check if variable exists
std::string config_path = hshm::SystemInfo::Getenv("MY_APP_CONFIG");
if (config_path.empty()) {
    printf("MY_APP_CONFIG not set, using default\n");
    config_path = "/etc/myapp/default.conf";
}

// Get with size limit (important for potentially large variables)
size_t max_size = hshm::Unit<size_t>::Megabytes(1);
std::string large_var = hshm::SystemInfo::Getenv("LARGE_DATA", max_size);
```

### Setting Environment Variables

```cpp
// Set environment variables with overwrite flag
hshm::SystemInfo::Setenv("MY_APP_VERSION", "2.1.0", 1);        // overwrite=1 (always set)
hshm::SystemInfo::Setenv("MY_APP_DEBUG", "true", 0);           // overwrite=0 (don't overwrite if exists)
hshm::SystemInfo::Setenv("MY_APP_LOG_LEVEL", "INFO", 1);

// Setting paths
std::string app_home = "/opt/myapp";
hshm::SystemInfo::Setenv("MY_APP_HOME", app_home, 1);
hshm::SystemInfo::Setenv("MY_APP_CONFIG", app_home + "/config", 1);
hshm::SystemInfo::Setenv("MY_APP_DATA", app_home + "/data", 1);

// Setting numeric values
hshm::SystemInfo::Setenv("MAX_THREADS", std::to_string(8), 1);
hshm::SystemInfo::Setenv("BUFFER_SIZE", std::to_string(1024*1024), 1);
```

### Unsetting Environment Variables

```cpp
// Remove environment variables
hshm::SystemInfo::Unsetenv("TEMP_VAR");
hshm::SystemInfo::Unsetenv("OLD_CONFIG");
hshm::SystemInfo::Unsetenv("DEPRECATED_OPTION");

// Clean up temporary variables
std::vector<std::string> temp_vars = {
    "TMP_BUILD_DIR",
    "TMP_CACHE",
    "TMP_SESSION_ID"
};

for (const auto& var : temp_vars) {
    hshm::SystemInfo::Unsetenv(var.c_str());
}
```

## Configuration from Environment

### Application Configuration Class

```cpp
class AppConfiguration {
private:
    std::string config_dir_;
    std::string data_dir_;
    std::string log_file_;
    int log_level_;
    bool debug_mode_;
    size_t max_memory_;
    int thread_count_;
    
public:
    void LoadFromEnvironment() {
        // Configuration directory with XDG compliance
        config_dir_ = GetConfigDirectory();
        
        // Data directory with fallback chain
        data_dir_ = GetDataDirectory();
        
        // Logging configuration
        ConfigureLogging();
        
        // Runtime parameters
        ConfigureRuntime();
        
        // Display loaded configuration
        DisplayConfiguration();
    }
    
private:
    std::string GetConfigDirectory() {
        // Priority: APP_CONFIG_DIR > XDG_CONFIG_HOME > HOME/.config
        std::string dir = hshm::SystemInfo::Getenv("APP_CONFIG_DIR");
        if (!dir.empty()) return dir;
        
        dir = hshm::SystemInfo::Getenv("XDG_CONFIG_HOME");
        if (!dir.empty()) return dir + "/myapp";
        
        std::string home = hshm::SystemInfo::Getenv("HOME");
        if (!home.empty()) return home + "/.config/myapp";
        
        return "/etc/myapp";  // System fallback
    }
    
    std::string GetDataDirectory() {
        // Priority: APP_DATA_DIR > XDG_DATA_HOME > HOME/.local/share
        std::string dir = hshm::SystemInfo::Getenv("APP_DATA_DIR");
        if (!dir.empty()) return dir;
        
        dir = hshm::SystemInfo::Getenv("XDG_DATA_HOME");
        if (!dir.empty()) return dir + "/myapp";
        
        std::string home = hshm::SystemInfo::Getenv("HOME");
        if (!home.empty()) return home + "/.local/share/myapp";
        
        return "/var/lib/myapp";  // System fallback
    }
    
    void ConfigureLogging() {
        // Log file location
        log_file_ = hshm::SystemInfo::Getenv("APP_LOG_FILE");
        if (log_file_.empty()) {
            std::string log_dir = hshm::SystemInfo::Getenv("APP_LOG_DIR");
            if (log_dir.empty()) {
                log_dir = "/var/log";
            }
            log_file_ = log_dir + "/myapp.log";
        }
        
        // Log level parsing
        std::string level_str = hshm::SystemInfo::Getenv("APP_LOG_LEVEL");
        log_level_ = ParseLogLevel(level_str);
        
        // Debug mode
        std::string debug_str = hshm::SystemInfo::Getenv("APP_DEBUG");
        debug_mode_ = IsTrue(debug_str);
    }
    
    void ConfigureRuntime() {
        // Memory limit
        std::string mem_str = hshm::SystemInfo::Getenv("APP_MAX_MEMORY");
        if (!mem_str.empty()) {
            max_memory_ = ParseSize(mem_str);
        } else {
            max_memory_ = hshm::Unit<size_t>::Gigabytes(1);  // Default 1GB
        }
        
        // Thread count
        std::string thread_str = hshm::SystemInfo::Getenv("APP_THREADS");
        if (!thread_str.empty()) {
            thread_count_ = std::stoi(thread_str);
        } else {
            thread_count_ = std::thread::hardware_concurrency();
        }
    }
    
    int ParseLogLevel(const std::string& level) {
        if (level == "ERROR" || level == "0") return 0;
        if (level == "WARNING" || level == "1") return 1;
        if (level == "INFO" || level == "2") return 2;
        if (level == "DEBUG" || level == "3") return 3;
        if (level == "TRACE" || level == "4") return 4;
        return 2;  // Default to INFO
    }
    
    bool IsTrue(const std::string& value) {
        return value == "1" || value == "true" || 
               value == "TRUE" || value == "yes" || 
               value == "YES" || value == "on" || value == "ON";
    }
    
    size_t ParseSize(const std::string& size_str) {
        // Simple size parsing (enhance as needed)
        size_t value = std::stoull(size_str);
        if (size_str.find("K") != std::string::npos) value *= 1024;
        if (size_str.find("M") != std::string::npos) value *= 1024*1024;
        if (size_str.find("G") != std::string::npos) value *= 1024*1024*1024;
        return value;
    }
    
    void DisplayConfiguration() {
        printf("Application Configuration (from environment):\n");
        printf("  Config Dir: %s\n", config_dir_.c_str());
        printf("  Data Dir: %s\n", data_dir_.c_str());
        printf("  Log File: %s\n", log_file_.c_str());
        printf("  Log Level: %d\n", log_level_);
        printf("  Debug Mode: %s\n", debug_mode_ ? "enabled" : "disabled");
        printf("  Max Memory: %zu MB\n", max_memory_ / (1024*1024));
        printf("  Thread Count: %d\n", thread_count_);
    }
    
public:
    // Getters for configuration values
    const std::string& GetConfigDir() const { return config_dir_; }
    const std::string& GetDataDir() const { return data_dir_; }
    const std::string& GetLogFile() const { return log_file_; }
    int GetLogLevel() const { return log_level_; }
    bool IsDebugMode() const { return debug_mode_; }
    size_t GetMaxMemory() const { return max_memory_; }
    int GetThreadCount() const { return thread_count_; }
};
```

## Environment Variable Expansion

### Basic Variable Expansion

```cpp
class EnvironmentExpander {
public:
    // Expand ${VAR} patterns in strings
    static std::string ExpandVariables(const std::string& input) {
        std::string result = input;
        size_t pos = 0;
        
        while ((pos = result.find("${", pos)) != std::string::npos) {
            size_t end = result.find("}", pos);
            if (end == std::string::npos) break;
            
            std::string var_name = result.substr(pos + 2, end - pos - 2);
            std::string var_value = hshm::SystemInfo::Getenv(var_name);
            
            result.replace(pos, end - pos + 1, var_value);
            pos += var_value.length();
        }
        
        return result;
    }
    
    // Expand $VAR patterns (without braces)
    static std::string ExpandSimpleVariables(const std::string& input) {
        std::string result = input;
        size_t pos = 0;
        
        while ((pos = result.find("$", pos)) != std::string::npos) {
            if (pos + 1 < result.length() && result[pos + 1] == '{') {
                pos++;  // Skip ${} patterns
                continue;
            }
            
            size_t end = pos + 1;
            while (end < result.length() && 
                   (std::isalnum(result[end]) || result[end] == '_')) {
                end++;
            }
            
            if (end > pos + 1) {
                std::string var_name = result.substr(pos + 1, end - pos - 1);
                std::string var_value = hshm::SystemInfo::Getenv(var_name);
                result.replace(pos, end - pos, var_value);
                pos += var_value.length();
            } else {
                pos++;
            }
        }
        
        return result;
    }
};

// Usage examples
std::string path1 = EnvironmentExpander::ExpandVariables("${HOME}/data/${USER}/files");
std::string path2 = EnvironmentExpander::ExpandSimpleVariables("$HOME/data/$USER/files");
```

### Advanced Expansion with Defaults

```cpp
class AdvancedEnvironmentExpander {
public:
    // Expand with default values: ${VAR:-default}
    static std::string ExpandWithDefaults(const std::string& input) {
        std::string result = input;
        size_t pos = 0;
        
        while ((pos = result.find("${", pos)) != std::string::npos) {
            size_t end = result.find("}", pos);
            if (end == std::string::npos) break;
            
            std::string var_expr = result.substr(pos + 2, end - pos - 2);
            std::string var_name, default_value;
            
            size_t default_pos = var_expr.find(":-");
            if (default_pos != std::string::npos) {
                var_name = var_expr.substr(0, default_pos);
                default_value = var_expr.substr(default_pos + 2);
            } else {
                var_name = var_expr;
            }
            
            std::string var_value = hshm::SystemInfo::Getenv(var_name);
            if (var_value.empty() && !default_value.empty()) {
                var_value = default_value;
            }
            
            result.replace(pos, end - pos + 1, var_value);
            pos += var_value.length();
        }
        
        return result;
    }
    
    // Expand with alternative: ${VAR:+alternative}
    static std::string ExpandWithAlternative(const std::string& input) {
        std::string result = input;
        size_t pos = 0;
        
        while ((pos = result.find("${", pos)) != std::string::npos) {
            size_t end = result.find("}", pos);
            if (end == std::string::npos) break;
            
            std::string var_expr = result.substr(pos + 2, end - pos - 2);
            std::string var_name, alt_value;
            
            size_t alt_pos = var_expr.find(":+");
            if (alt_pos != std::string::npos) {
                var_name = var_expr.substr(0, alt_pos);
                alt_value = var_expr.substr(alt_pos + 2);
            } else {
                var_name = var_expr;
            }
            
            std::string var_value = hshm::SystemInfo::Getenv(var_name);
            if (!var_value.empty() && !alt_value.empty()) {
                var_value = alt_value;  // Use alternative if var is set
            }
            
            result.replace(pos, end - pos + 1, var_value);
            pos += var_value.length();
        }
        
        return result;
    }
};

// Usage examples
std::string config = AdvancedEnvironmentExpander::ExpandWithDefaults(
    "${CONFIG_DIR:-/etc/myapp}/config.yaml"
);
std::string message = AdvancedEnvironmentExpander::ExpandWithAlternative(
    "${DEBUG:+Debug mode is enabled}"
);
```

## Environment Setup Patterns

### Application Environment Initialization

```cpp
class EnvironmentSetup {
public:
    static void InitializeApplicationEnvironment(const std::string& app_name) {
        // Set application identification
        hshm::SystemInfo::Setenv("APP_NAME", app_name, 1);
        hshm::SystemInfo::Setenv("APP_VERSION", GetVersion(), 1);
        hshm::SystemInfo::Setenv("APP_PID", std::to_string(getpid()), 1);
        
        // Set up directory structure
        SetupDirectories(app_name);
        
        // Configure runtime paths
        ConfigureRuntimePaths();
        
        // Set up locale if not set
        ConfigureLocale();
        
        // Set process-specific variables
        SetProcessVariables();
        
        printf("Environment initialized for %s\n", app_name.c_str());
    }
    
private:
    static std::string GetVersion() {
        // Read from version file or return compiled version
        return "2.1.0";
    }
    
    static void SetupDirectories(const std::string& app_name) {
        std::string home = hshm::SystemInfo::Getenv("HOME");
        if (home.empty()) home = "/tmp";
        
        std::string app_home = home + "/." + app_name;
        hshm::SystemInfo::Setenv("APP_HOME", app_home, 1);
        hshm::SystemInfo::Setenv("APP_CONFIG_DIR", app_home + "/config", 0);
        hshm::SystemInfo::Setenv("APP_DATA_DIR", app_home + "/data", 0);
        hshm::SystemInfo::Setenv("APP_CACHE_DIR", app_home + "/cache", 0);
        hshm::SystemInfo::Setenv("APP_LOG_DIR", app_home + "/logs", 0);
        hshm::SystemInfo::Setenv("APP_TMP_DIR", app_home + "/tmp", 0);
    }
    
    static void ConfigureRuntimePaths() {
        // Get executable path (platform-specific)
        std::string exe_path = GetExecutablePath();
        std::string exe_dir = GetDirectoryFromPath(exe_path);
        
        hshm::SystemInfo::Setenv("APP_BIN_DIR", exe_dir, 1);
        hshm::SystemInfo::Setenv("APP_LIB_DIR", exe_dir + "/../lib", 1);
        hshm::SystemInfo::Setenv("APP_SHARE_DIR", exe_dir + "/../share", 1);
        
        // Update library path
        UpdateLibraryPath(exe_dir + "/../lib");
    }
    
    static void ConfigureLocale() {
        if (hshm::SystemInfo::Getenv("LANG").empty()) {
            hshm::SystemInfo::Setenv("LANG", "en_US.UTF-8", 1);
        }
        if (hshm::SystemInfo::Getenv("LC_ALL").empty()) {
            hshm::SystemInfo::Setenv("LC_ALL", "C", 0);
        }
    }
    
    static void SetProcessVariables() {
        // Set process start time
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        hshm::SystemInfo::Setenv("APP_START_TIME", std::to_string(timestamp), 1);
        
        // Set hostname
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            hshm::SystemInfo::Setenv("APP_HOSTNAME", hostname, 1);
        }
        
        // Set user info
        hshm::SystemInfo::Setenv("APP_UID", std::to_string(getuid()), 1);
        hshm::SystemInfo::Setenv("APP_GID", std::to_string(getgid()), 1);
    }
    
    static std::string GetExecutablePath() {
#ifdef __linux__
        char path[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path)-1);
        if (len != -1) {
            path[len] = '\0';
            return std::string(path);
        }
#elif __APPLE__
        char path[PATH_MAX];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            return std::string(path);
        }
#endif
        return "";
    }
    
    static std::string GetDirectoryFromPath(const std::string& path) {
        size_t pos = path.find_last_of("/\\");
        if (pos != std::string::npos) {
            return path.substr(0, pos);
        }
        return ".";
    }
    
    static void UpdateLibraryPath(const std::string& new_path) {
        std::string current_ld_path = hshm::SystemInfo::Getenv("LD_LIBRARY_PATH");
        std::string updated_path = new_path;
        if (!current_ld_path.empty()) {
            updated_path += ":" + current_ld_path;
        }
        hshm::SystemInfo::Setenv("LD_LIBRARY_PATH", updated_path, 1);
        
#ifdef __APPLE__
        // Also update DYLD_LIBRARY_PATH on macOS
        std::string current_dyld = hshm::SystemInfo::Getenv("DYLD_LIBRARY_PATH");
        std::string updated_dyld = new_path;
        if (!current_dyld.empty()) {
            updated_dyld += ":" + current_dyld;
        }
        hshm::SystemInfo::Setenv("DYLD_LIBRARY_PATH", updated_dyld, 1);
#endif
    }
};
```

## Environment Variable Security

```cpp
class SecureEnvironment {
public:
    // Remove sensitive variables
    static void ClearSensitiveVariables() {
        std::vector<std::string> sensitive_vars = {
            "PASSWORD",
            "SECRET_KEY",
            "API_TOKEN",
            "DB_PASSWORD",
            "PRIVATE_KEY",
            "CREDENTIALS"
        };
        
        for (const auto& var : sensitive_vars) {
            // Check for common prefixes
            for (const auto& prefix : {"", "APP_", "MY_", "SYSTEM_"}) {
                std::string full_var = prefix + var;
                hshm::SystemInfo::Unsetenv(full_var.c_str());
            }
        }
    }
    
    // Save and restore environment
    static std::map<std::string, std::string> SaveEnvironment(
        const std::vector<std::string>& vars) {
        std::map<std::string, std::string> saved;
        
        for (const auto& var : vars) {
            std::string value = hshm::SystemInfo::Getenv(var);
            if (!value.empty()) {
                saved[var] = value;
            }
        }
        
        return saved;
    }
    
    static void RestoreEnvironment(
        const std::map<std::string, std::string>& saved) {
        for (const auto& [var, value] : saved) {
            hshm::SystemInfo::Setenv(var.c_str(), value, 1);
        }
    }
    
    // Create isolated environment
    static void CreateIsolatedEnvironment() {
        // Clear all non-essential variables
        extern char **environ;
        if (environ) {
            std::vector<std::string> to_remove;
            for (char **env = environ; *env; env++) {
                std::string var(*env);
                size_t eq_pos = var.find('=');
                if (eq_pos != std::string::npos) {
                    std::string name = var.substr(0, eq_pos);
                    // Keep only essential variables
                    if (!IsEssentialVariable(name)) {
                        to_remove.push_back(name);
                    }
                }
            }
            
            for (const auto& var : to_remove) {
                hshm::SystemInfo::Unsetenv(var.c_str());
            }
        }
        
        // Set minimal environment
        hshm::SystemInfo::Setenv("PATH", "/usr/bin:/bin", 1);
        hshm::SystemInfo::Setenv("HOME", "/tmp", 1);
        hshm::SystemInfo::Setenv("USER", "nobody", 1);
    }
    
private:
    static bool IsEssentialVariable(const std::string& name) {
        static const std::set<std::string> essential = {
            "PATH", "HOME", "USER", "SHELL", "TERM",
            "LANG", "LC_ALL", "TZ", "TMPDIR"
        };
        return essential.count(name) > 0;
    }
};
```

## Complete Example: Environment-Driven Application

```cpp
#include "hermes_shm/introspect/system_info.h"
#include <iostream>
#include <map>

class EnvironmentDrivenApp {
    AppConfiguration config_;
    std::map<std::string, std::string> original_env_;
    
public:
    int Run() {
        try {
            // Save original environment
            SaveOriginalEnvironment();
            
            // Initialize application environment
            InitializeEnvironment();
            
            // Load configuration from environment
            config_.LoadFromEnvironment();
            
            // Validate environment
            if (!ValidateEnvironment()) {
                std::cerr << "Environment validation failed\n";
                return 1;
            }
            
            // Run application
            return RunApplication();
            
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }
    
private:
    void SaveOriginalEnvironment() {
        // Save important variables
        std::vector<std::string> important_vars = {
            "PATH", "LD_LIBRARY_PATH", "HOME", "USER"
        };
        
        for (const auto& var : important_vars) {
            std::string value = hshm::SystemInfo::Getenv(var);
            if (!value.empty()) {
                original_env_[var] = value;
            }
        }
    }
    
    void InitializeEnvironment() {
        // Set application-specific environment
        EnvironmentSetup::InitializeApplicationEnvironment("myapp");
        
        // Set feature flags from command line or config
        SetFeatureFlags();
        
        // Configure debugging
        ConfigureDebugging();
    }
    
    void SetFeatureFlags() {
        // Enable features based on environment
        std::string features = hshm::SystemInfo::Getenv("APP_FEATURES");
        if (features.find("experimental") != std::string::npos) {
            hshm::SystemInfo::Setenv("ENABLE_EXPERIMENTAL", "1", 1);
        }
        if (features.find("verbose") != std::string::npos) {
            hshm::SystemInfo::Setenv("VERBOSE_LOGGING", "1", 1);
        }
        if (features.find("profiling") != std::string::npos) {
            hshm::SystemInfo::Setenv("ENABLE_PROFILING", "1", 1);
        }
    }
    
    void ConfigureDebugging() {
        if (config_.IsDebugMode()) {
            hshm::SystemInfo::Setenv("MALLOC_CHECK_", "3", 1);  // glibc malloc debugging
            hshm::SystemInfo::Setenv("G_DEBUG", "fatal-warnings", 1);  // GLib debugging
        }
    }
    
    bool ValidateEnvironment() {
        // Check required variables
        std::vector<std::string> required = {
            "APP_HOME", "APP_CONFIG_DIR", "APP_DATA_DIR"
        };
        
        for (const auto& var : required) {
            if (hshm::SystemInfo::Getenv(var).empty()) {
                std::cerr << "Required variable " << var << " not set\n";
                return false;
            }
        }
        
        // Validate paths exist
        std::string config_dir = hshm::SystemInfo::Getenv("APP_CONFIG_DIR");
        if (!DirectoryExists(config_dir)) {
            std::cerr << "Config directory does not exist: " << config_dir << "\n";
            return false;
        }
        
        return true;
    }
    
    int RunApplication() {
        printf("Application running with configuration:\n");
        printf("  Config Dir: %s\n", config_.GetConfigDir().c_str());
        printf("  Data Dir: %s\n", config_.GetDataDir().c_str());
        printf("  Debug Mode: %s\n", config_.IsDebugMode() ? "ON" : "OFF");
        printf("  Max Memory: %zu MB\n", config_.GetMaxMemory() / (1024*1024));
        printf("  Threads: %d\n", config_.GetThreadCount());
        
        // Main application logic here...
        
        return 0;
    }
    
    bool DirectoryExists(const std::string& path) {
        struct stat st;
        return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }
    
public:
    ~EnvironmentDrivenApp() {
        // Restore original environment if needed
        for (const auto& [var, value] : original_env_) {
            hshm::SystemInfo::Setenv(var.c_str(), value, 1);
        }
    }
};

int main() {
    EnvironmentDrivenApp app;
    return app.Run();
}
```

## Best Practices

1. **Namespace Variables**: Use application-specific prefixes (e.g., `APP_`, `MYAPP_`) to avoid conflicts
2. **Default Values**: Always provide sensible defaults when environment variables are not set
3. **Size Limits**: Use size limits when reading potentially large environment variables
4. **Security**: Never store passwords or sensitive data in environment variables
5. **Documentation**: Document all environment variables your application uses
6. **Validation**: Validate environment variable values before use
7. **XDG Compliance**: Follow XDG Base Directory specification for Unix systems
8. **Cleanup**: Unset temporary variables when no longer needed
9. **Overwrite Policy**: Be careful with the overwrite flag when setting variables
10. **Platform Awareness**: Consider platform differences in environment variable handling