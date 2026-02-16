@CLAUDE.md 

We will now implement the base classes for parsing the omni file. Use the cpp agent for this.
Focus on getting compiling. Do not write stub code

The omni format is a file format that describes how to ingest data and the semantics of the data. 
It can download data from remote repos into local filesystems and from local filesystems into iowarp.

Below is an example of an omni file for files
```yaml
# This will download data from an external repository to local filesystem
- src: globus::/somefile.bin
  dst: /path/to/somefile.bin
# This will ingest data from local filesystem into iowarp
- src: file::/path/to/somefile.bin
  dst: iowarp::example
  format: tensor<float, 10, 10, 10> # Indicates the format of the data being assimilated
  depends_on: downloader
```

## Assimilation Context
```cpp
struct AssimilationCtx {
    std::string src;
    std::string dst;
    std::string format;
    std::string depends_on;
    size_t range_off, range_size;
}
```

The set of all keys that could go into a single entry of the omni file. 

## Base Assimilator

```cpp
class BaseAssimilator {
 public:
  // Produce AssimilateData tasks
  virtual int Schedule(const AssimilationCtx &ctx) = 0;
}
```

## Assimilator Factory

```cpp
class AssimilatorFactory {
 public:
  std::unique_ptr<BaseAssimilator> Get(std::string src) {
    // Get the part before the first :: to select the assimilator
  }
}
```

## Create (core_runtime.cc)

Create the connection the the content transfer engine. Create a client
with fixed pool id from cte headers. The name is kCtePoolId

```
namespace wrp_cte::core {

// CTE Core Pool ID constant (major: 512, minor: 0)
static constexpr chi::PoolId kCtePoolId(512, 0);

```

## ParseOmni (core_runtime.cc)

We will update ParseOmni in core_runtime.cc to use the assimilator factory.
This will call the Schedule function for the particular assimilation context.

Update the ParseOmni task to take as inpute an AssimilationCtx. Since this 
has std:: data structures, we should serialize it using cereal first and store
the serialized context in a hshm::priv::string.

### Binary File Assimilator

Parse the part of dst before the "::" to see where to store data. 
Currently, only iowarp should be supported.

```cpp
int Schedule(const AssimilationCtx &ctx) {
    if (GetUrlProtocol(ctx.dst) != "iowarp") {
        return -1;
    }

    // Create an iowarp tag using the part after :: in the url
    cte_.GetOrCreateTag(GetUrlPath(ctx.dst));

    if (ctx.depends_on.empty()) {
        // Get file size
        // Divide file into chunks, up to 1MB each
        // Submit up to 32 tasks in parallel at a time
        // Repeat batching until tasks compelted
    } else {
        // Placeholder for now
    }
}
```

Remove AssimilateData API from core_runtime.cc