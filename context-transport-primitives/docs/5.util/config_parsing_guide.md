# HSHM Configuration Parsing Guide

## Overview

The Configuration Parsing API in Hermes Shared Memory (HSHM) provides powerful utilities for parsing configuration files, processing hostnames, and converting human-readable units. The `ConfigParse` class and `BaseConfig` abstract class form the foundation for flexible configuration management.

## Basic Configuration with YAML

### Creating a Configuration Class

```cpp
#include "hermes_shm/util/config_parse.h"
#include "yaml-cpp/yaml.h"

class ApplicationConfig : public hshm::BaseConfig {
public:
    // Configuration fields
    std::string server_address;
    int port;
    size_t buffer_size;
    double timeout_seconds;
    std::vector<std::string> allowed_hosts;
    std::map<std::string, std::string> features;
    
    // Required: Set default values
    void LoadDefault() override {
        server_address = "localhost";
        port = 8080;
        buffer_size = hshm::Unit<size_t>::Megabytes(1);
        timeout_seconds = 30.0;
        allowed_hosts.clear();
        features.clear();
    }
    
private:
    // Required: Parse YAML configuration
    void ParseYAML(YAML::Node &yaml_conf) override {
        if (yaml_conf["server"]) {
            auto server = yaml_conf["server"];
            if (server["address"]) {
                server_address = server["address"].as<std::string>();
            }
            if (server["port"]) {
                port = server["port"].as<int>();
            }
        }
        
        if (yaml_conf["buffer_size"]) {
            std::string size_str = yaml_conf["buffer_size"].as<std::string>();
            buffer_size = hshm::ConfigParse::ParseSize(size_str);
        }
        
        if (yaml_conf["timeout"]) {
            timeout_seconds = yaml_conf["timeout"].as<double>();
        }
        
        if (yaml_conf["allowed_hosts"]) {
            ParseHostList(yaml_conf["allowed_hosts"]);
        }
        
        if (yaml_conf["features"]) {
            for (auto it = yaml_conf["features"].begin(); 
                 it != yaml_conf["features"].end(); ++it) {
                features[it->first.as<std::string>()] = it->second.as<std::string>();
            }
        }
    }
    
    void ParseHostList(YAML::Node hosts_node) {
        allowed_hosts.clear();
        for (auto host_node : hosts_node) {
            std::string host_pattern = host_node.as<std::string>();
            // Expand hostname patterns
            hshm::ConfigParse::ParseHostNameString(host_pattern, allowed_hosts);
        }
    }
};
```

### Loading Configuration

```cpp
// Example YAML configuration file: config.yaml
/*
server:
  address: "0.0.0.0"
  port: 9090

buffer_size: "2GB"
timeout: 60.0

allowed_hosts:
  - "compute[01-10]-ib"
  - "storage[001-003]"
  - "login1;login2"

features:
  compression: "enabled"
  encryption: "aes256"
  cache_size: "512MB"
*/

ApplicationConfig config;

// Load from file with defaults
config.LoadFromFile("/path/to/config.yaml");

// Load from file without defaults
config.LoadFromFile("/path/to/config.yaml", false);

// Load from string
std::string yaml_content = R"(
server:
  address: "192.168.1.100"
  port: 8888
buffer_size: "512MB"
)";
config.LoadText(yaml_content);

// Access configuration values
printf("Server: %s:%d\n", config.server_address.c_str(), config.port);
printf("Buffer Size: %zu bytes\n", config.buffer_size);
printf("Hosts: %zu allowed\n", config.allowed_hosts.size());
```

## Hostname Parsing

### Basic Hostname Expansion

```cpp
std::vector<std::string> hosts;

// Simple range expansion
hshm::ConfigParse::ParseHostNameString("node[01-05]", hosts);
// Result: node01, node02, node03, node04, node05

// Multiple ranges with prefix and suffix
hosts.clear();
hshm::ConfigParse::ParseHostNameString("compute[001-003,010-012]-40g", hosts);
// Result: compute001-40g, compute002-40g, compute003-40g,
//         compute010-40g, compute011-40g, compute012-40g

// Semicolon separation for different patterns
hosts.clear();
hshm::ConfigParse::ParseHostNameString("gpu[01-02]-ib;cpu[01-03]-eth", hosts);
// Result: gpu01-ib, gpu02-ib, cpu01-eth, cpu02-eth, cpu03-eth

// Single values in ranges
hosts.clear();
hshm::ConfigParse::ParseHostNameString("special[1,5,9,10]", hosts);
// Result: special1, special5, special9, special10
```

### Advanced Hostname Patterns

```cpp
class ClusterConfig {
    std::vector<std::string> compute_nodes_;
    std::vector<std::string> storage_nodes_;
    std::vector<std::string> management_nodes_;
    
public:
    void ParseClusterTopology(const std::string& topology_file) {
        YAML::Node topology = YAML::LoadFile(topology_file);
        
        // Parse different node types with complex patterns
        if (topology["compute"]) {
            std::string pattern = topology["compute"].as<std::string>();
            hshm::ConfigParse::ParseHostNameString(pattern, compute_nodes_);
        }
        
        if (topology["storage"]) {
            std::string pattern = topology["storage"].as<std::string>();
            hshm::ConfigParse::ParseHostNameString(pattern, storage_nodes_);
        }
        
        if (topology["management"]) {
            std::string pattern = topology["management"].as<std::string>();
            hshm::ConfigParse::ParseHostNameString(pattern, management_nodes_);
        }
        
        DisplayTopology();
    }
    
    void DisplayTopology() {
        printf("Cluster Topology:\n");
        printf("  Compute Nodes (%zu):\n", compute_nodes_.size());
        for (size_t i = 0; i < std::min(size_t(5), compute_nodes_.size()); ++i) {
            printf("    %s\n", compute_nodes_[i].c_str());
        }
        if (compute_nodes_.size() > 5) {
            printf("    ... and %zu more\n", compute_nodes_.size() - 5);
        }
        
        printf("  Storage Nodes (%zu):\n", storage_nodes_.size());
        for (const auto& node : storage_nodes_) {
            printf("    %s\n", node.c_str());
        }
        
        printf("  Management Nodes (%zu):\n", management_nodes_.size());
        for (const auto& node : management_nodes_) {
            printf("    %s\n", node.c_str());
        }
    }
};

// Example topology.yaml:
/*
compute: "cn[001-128]-ib"
storage: "st[01-08]-40g"
management: "mgmt[1-2];login[1-2];scheduler"
*/
```

### Hostfile Processing

```cpp
// Parse a hostfile with multiple formats
std::vector<std::string> ParseHostfile(const std::string& hostfile_path) {
    std::vector<std::string> all_hosts = hshm::ConfigParse::ParseHostfile(hostfile_path);
    
    // Process and validate hosts
    std::vector<std::string> valid_hosts;
    for (const auto& host : all_hosts) {
        if (IsValidHostname(host)) {
            valid_hosts.push_back(host);
        } else {
            fprintf(stderr, "Warning: Invalid hostname '%s' skipped\n", host.c_str());
        }
    }
    
    return valid_hosts;
}

bool IsValidHostname(const std::string& hostname) {
    // Basic validation
    if (hostname.empty() || hostname.length() > 255) {
        return false;
    }
    
    // Check for valid characters
    for (char c : hostname) {
        if (!std::isalnum(c) && c != '-' && c != '.') {
            return false;
        }
    }
    
    return true;
}

// Example hostfile content:
/*
# Compute nodes
compute[001-064]-ib
compute[065-128]-ib

# GPU nodes  
gpu[01-16]-40g

# Special nodes
login1
login2
scheduler
storage[01-04]
*/
```

## Size and Unit Parsing

### Memory Size Parsing

```cpp
// Parse various memory size formats
size_t size1 = hshm::ConfigParse::ParseSize("1024");        // 1024 bytes
size_t size2 = hshm::ConfigParse::ParseSize("4K");          // 4 KB = 4096 bytes
size_t size3 = hshm::ConfigParse::ParseSize("4KB");         // 4 KB = 4096 bytes
size_t size4 = hshm::ConfigParse::ParseSize("2.5M");        // 2.5 MB
size_t size5 = hshm::ConfigParse::ParseSize("1.5GB");       // 1.5 GB
size_t size6 = hshm::ConfigParse::ParseSize("2T");          // 2 TB
size_t size7 = hshm::ConfigParse::ParseSize("0.5PB");       // 0.5 PB
size_t size_inf = hshm::ConfigParse::ParseSize("inf");      // Maximum size_t value

printf("Parsed sizes:\n");
printf("  4K = %zu bytes\n", size2);
printf("  2.5M = %zu bytes (%.2f MB)\n", size4, size4 / (1024.0 * 1024.0));
printf("  1.5GB = %zu bytes\n", size5);
printf("  inf = %zu (max value)\n", size_inf);
```

### Bandwidth Parsing

```cpp
// Parse bandwidth specifications (bytes per second)
size_t bw1 = hshm::ConfigParse::ParseBandwidth("100MB");    // 100 MB/s
size_t bw2 = hshm::ConfigParse::ParseBandwidth("10GB");     // 10 GB/s
size_t bw3 = hshm::ConfigParse::ParseBandwidth("1.5TB");    // 1.5 TB/s

// Note: ParseBandwidth currently treats input as bytes/second
// Additional parsing for "Gbps", "MB/s" etc. would need custom implementation
```

### Latency Parsing

```cpp
// Parse latency values (returns nanoseconds)
size_t lat1 = hshm::ConfigParse::ParseLatency("100n");      // 100 nanoseconds
size_t lat2 = hshm::ConfigParse::ParseLatency("50u");       // 50 microseconds
size_t lat3 = hshm::ConfigParse::ParseLatency("10m");       // 10 milliseconds  
size_t lat4 = hshm::ConfigParse::ParseLatency("1s");        // 1 second

printf("Latencies in nanoseconds:\n");
printf("  100n = %zu ns\n", lat1);
printf("  50u = %zu ns (%.3f μs)\n", lat2, lat2 / 1000.0);
printf("  10m = %zu ns (%.3f ms)\n", lat3, lat3 / 1000000.0);
printf("  1s = %zu ns (%.3f s)\n", lat4, lat4 / 1000000000.0);
```

### Custom Number Parsing

```cpp
// Parse numbers with generic types
int int_val = hshm::ConfigParse::ParseNumber<int>("42");
double double_val = hshm::ConfigParse::ParseNumber<double>("3.14159");
float float_val = hshm::ConfigParse::ParseNumber<float>("2.718");
long long_val = hshm::ConfigParse::ParseNumber<long>("1234567890");

// Special infinity value
double inf_double = hshm::ConfigParse::ParseNumber<double>("inf");
int inf_int = hshm::ConfigParse::ParseNumber<int>("inf");  // Returns INT_MAX

// Extract suffixes from number strings
std::string suffix1 = hshm::ConfigParse::ParseNumberSuffix("100MB");   // "MB"
std::string suffix2 = hshm::ConfigParse::ParseNumberSuffix("3.14");    // ""
std::string suffix3 = hshm::ConfigParse::ParseNumberSuffix("50ms");    // "ms"
std::string suffix4 = hshm::ConfigParse::ParseNumberSuffix("1.5GHz");  // "GHz"
```

## Path Expansion

### Environment Variable Expansion

```cpp
// Expand environment variables in paths
std::string ExpandConfigPath(const std::string& template_path) {
    return hshm::ConfigParse::ExpandPath(template_path);
}

// Examples
std::string home_config = ExpandConfigPath("${HOME}/.config/myapp");
std::string data_path = ExpandConfigPath("${XDG_DATA_HOME}/myapp/data");
std::string temp_file = ExpandConfigPath("${TMPDIR}/myapp_${USER}.tmp");

// Complex expansion with multiple variables
std::string complex = ExpandConfigPath(
    "${HOME}/.cache/${APPLICATION_NAME}-${VERSION}/data"
);

// Set up environment and expand
hshm::SystemInfo::Setenv("APP_ROOT", "/opt/myapp", 1);
hshm::SystemInfo::Setenv("APP_VERSION", "2.1.0", 1);
std::string app_config = ExpandConfigPath("${APP_ROOT}/config-${APP_VERSION}.yaml");
```

## Complex Configuration Example

### Distributed System Configuration

```cpp
class DistributedSystemConfig : public hshm::BaseConfig {
public:
    // Cluster configuration
    struct ClusterConfig {
        std::vector<std::string> nodes;
        std::string coordinator;
        int replication_factor;
    };
    
    // Storage configuration
    struct StorageConfig {
        size_t cache_size;
        size_t block_size;
        std::string data_directory;
        std::vector<std::string> storage_nodes;
    };
    
    // Network configuration
    struct NetworkConfig {
        size_t bandwidth_limit;
        size_t latency_ns;
        int port_range_start;
        int port_range_end;
    };
    
    ClusterConfig cluster;
    StorageConfig storage;
    NetworkConfig network;
    std::map<std::string, std::string> advanced_options;
    
    void LoadDefault() override {
        // Cluster defaults
        cluster.nodes.clear();
        cluster.coordinator = "localhost";
        cluster.replication_factor = 3;
        
        // Storage defaults
        storage.cache_size = hshm::Unit<size_t>::Gigabytes(1);
        storage.block_size = hshm::Unit<size_t>::Megabytes(1);
        storage.data_directory = "/var/lib/myapp";
        storage.storage_nodes.clear();
        
        // Network defaults
        network.bandwidth_limit = hshm::Unit<size_t>::Gigabytes(10);
        network.latency_ns = 1000000;  // 1ms
        network.port_range_start = 9000;
        network.port_range_end = 9100;
        
        advanced_options.clear();
    }
    
private:
    void ParseYAML(YAML::Node &yaml_conf) override {
        ParseCluster(yaml_conf["cluster"]);
        ParseStorage(yaml_conf["storage"]);
        ParseNetwork(yaml_conf["network"]);
        ParseAdvanced(yaml_conf["advanced"]);
    }
    
    void ParseCluster(YAML::Node node) {
        if (!node) return;
        
        if (node["nodes"]) {
            cluster.nodes.clear();
            for (auto n : node["nodes"]) {
                std::string pattern = n.as<std::string>();
                hshm::ConfigParse::ParseHostNameString(pattern, cluster.nodes);
            }
        }
        
        if (node["coordinator"]) {
            cluster.coordinator = node["coordinator"].as<std::string>();
        }
        
        if (node["replication_factor"]) {
            cluster.replication_factor = node["replication_factor"].as<int>();
        }
    }
    
    void ParseStorage(YAML::Node node) {
        if (!node) return;
        
        if (node["cache_size"]) {
            storage.cache_size = hshm::ConfigParse::ParseSize(
                node["cache_size"].as<std::string>());
        }
        
        if (node["block_size"]) {
            storage.block_size = hshm::ConfigParse::ParseSize(
                node["block_size"].as<std::string>());
        }
        
        if (node["data_directory"]) {
            storage.data_directory = hshm::ConfigParse::ExpandPath(
                node["data_directory"].as<std::string>());
        }
        
        if (node["storage_nodes"]) {
            storage.storage_nodes.clear();
            for (auto n : node["storage_nodes"]) {
                std::string pattern = n.as<std::string>();
                hshm::ConfigParse::ParseHostNameString(pattern, storage.storage_nodes);
            }
        }
    }
    
    void ParseNetwork(YAML::Node node) {
        if (!node) return;
        
        if (node["bandwidth_limit"]) {
            network.bandwidth_limit = hshm::ConfigParse::ParseBandwidth(
                node["bandwidth_limit"].as<std::string>());
        }
        
        if (node["latency"]) {
            network.latency_ns = hshm::ConfigParse::ParseLatency(
                node["latency"].as<std::string>());
        }
        
        if (node["port_range"]) {
            auto range = node["port_range"];
            if (range["start"]) {
                network.port_range_start = range["start"].as<int>();
            }
            if (range["end"]) {
                network.port_range_end = range["end"].as<int>();
            }
        }
    }
    
    void ParseAdvanced(YAML::Node node) {
        if (!node) return;
        
        for (auto it = node.begin(); it != node.end(); ++it) {
            std::string key = it->first.as<std::string>();
            std::string value = it->second.as<std::string>();
            
            // Expand environment variables in values
            value = hshm::ConfigParse::ExpandPath(value);
            advanced_options[key] = value;
        }
    }
    
public:
    void DisplayConfiguration() {
        printf("=== Distributed System Configuration ===\n");
        
        printf("\nCluster:\n");
        printf("  Nodes: %zu total\n", cluster.nodes.size());
        for (size_t i = 0; i < std::min(size_t(3), cluster.nodes.size()); ++i) {
            printf("    - %s\n", cluster.nodes[i].c_str());
        }
        if (cluster.nodes.size() > 3) {
            printf("    ... and %zu more\n", cluster.nodes.size() - 3);
        }
        printf("  Coordinator: %s\n", cluster.coordinator.c_str());
        printf("  Replication: %d\n", cluster.replication_factor);
        
        printf("\nStorage:\n");
        printf("  Cache Size: %.2f GB\n", storage.cache_size / (1024.0*1024.0*1024.0));
        printf("  Block Size: %.2f MB\n", storage.block_size / (1024.0*1024.0));
        printf("  Data Dir: %s\n", storage.data_directory.c_str());
        printf("  Storage Nodes: %zu\n", storage.storage_nodes.size());
        
        printf("\nNetwork:\n");
        printf("  Bandwidth: %.2f GB/s\n", 
               network.bandwidth_limit / (1024.0*1024.0*1024.0));
        printf("  Latency: %.3f ms\n", network.latency_ns / 1000000.0);
        printf("  Port Range: %d-%d\n", 
               network.port_range_start, network.port_range_end);
        
        if (!advanced_options.empty()) {
            printf("\nAdvanced Options:\n");
            for (const auto& [key, value] : advanced_options) {
                printf("  %s: %s\n", key.c_str(), value.c_str());
            }
        }
    }
};
```

### Example Configuration File

```yaml
# distributed_system.yaml
cluster:
  nodes:
    - "compute[001-032]-ib"
    - "compute[033-064]-ib"
  coordinator: "master01"
  replication_factor: 3

storage:
  cache_size: "16GB"
  block_size: "4MB"
  data_directory: "${DATA_ROOT}/distributed_storage"
  storage_nodes:
    - "storage[01-08]-40g"

network:
  bandwidth_limit: "40GB"
  latency: "100us"
  port_range:
    start: 9000
    end: 9500

advanced:
  compression: "lz4"
  encryption: "aes256"
  log_directory: "${LOG_ROOT}/distributed_system"
  checkpoint_interval: "300s"
  max_connections: "1000"
```

## Vector Parsing Utilities

```cpp
// Using BaseConfig's vector parsing helpers
class VectorConfig : public hshm::BaseConfig {
public:
    std::vector<int> integers;
    std::vector<double> doubles;
    std::vector<std::string> strings;
    std::list<std::string> string_list;
    
    void LoadDefault() override {
        integers = {1, 2, 3};
        doubles = {1.0, 2.0, 3.0};
        strings = {"default1", "default2"};
        string_list.clear();
    }
    
private:
    void ParseYAML(YAML::Node &yaml_conf) override {
        // Parse and append to existing vector
        if (yaml_conf["integers"]) {
            ParseVector<int>(yaml_conf["integers"], integers);
        }
        
        // Clear and parse vector
        if (yaml_conf["doubles"]) {
            ClearParseVector<double>(yaml_conf["doubles"], doubles);
        }
        
        // Parse strings
        if (yaml_conf["strings"]) {
            ClearParseVector<std::string>(yaml_conf["strings"], strings);
        }
        
        // Works with other STL containers too
        if (yaml_conf["string_list"]) {
            ClearParseVector<std::string>(yaml_conf["string_list"], string_list);
        }
    }
};
```

## Best Practices

1. **Default Values**: Always implement `LoadDefault()` with sensible defaults
2. **Environment Variables**: Use `ExpandPath()` for all file paths to support `${VAR}` expansion
3. **Size Parsing**: Use `ParseSize()` for memory/storage values for human-readable configs
4. **Hostname Patterns**: Leverage range syntax `[start-end]` for cluster configurations
5. **Error Handling**: Wrap configuration loading in try-catch blocks
6. **Validation**: Validate parsed values against system capabilities and constraints
7. **Documentation**: Document all configuration options and their formats
8. **Type Safety**: Use appropriate parsing functions for each data type
9. **Modularity**: Split large configurations into logical sections
10. **Version Control**: Consider configuration versioning for backward compatibility