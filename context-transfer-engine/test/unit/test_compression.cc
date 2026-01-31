/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Hermes. The full Hermes copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * CTE Compression Unit Tests
 *
 * This file contains tests for the compression path of iowarp CTE.
 * These tests are only compiled when WRP_CORE_ENABLE_COMPRESS is ON.
 *
 * Tests cover:
 * 1. Context struct - compression context creation and validation
 * 2. CompressionTelemetry - compression telemetry tracking
 * 3. CompressionStats - predicted compression statistics
 * 4. Compression library IDs and config IDs
 */

#include "simple_test.h"
#include <chrono>
#include <cmath>
#include <vector>

#include <chimaera/chimaera.h>
#include <wrp_cte/core/core_tasks.h>
#include <wrp_cte/core/core_runtime.h>

namespace fs = std::filesystem;

// Compression library IDs (from core_runtime.cc)
namespace CompressionLib {
  constexpr int BROTLI = 0;
  constexpr int BZIP2 = 1;
  constexpr int BLOSC2 = 2;
  constexpr int FPZIP = 3;
  constexpr int LZ4 = 4;
  constexpr int LZMA = 5;
  constexpr int SNAPPY = 6;
  constexpr int SZ3 = 7;
  constexpr int ZFP = 8;
  constexpr int ZLIB = 9;
  constexpr int ZSTD = 10;
}

// Compression config IDs (from core_runtime.cc)
namespace CompressionConfig {
  constexpr int BALANCED = 0;
  constexpr int BEST = 1;
  constexpr int DEFAULT = 2;
  constexpr int FAST = 3;
}

/**
 * Test Case 1: Context Struct Tests
 *
 * Verifies:
 * - Default construction with expected values
 * - Custom value assignment
 * - Dynamic compression modes (0=skip, 1=static, 2=dynamic)
 * - PSNR and performance settings
 */
TEST_CASE("Compression Context Creation", "[cte][compression][context]") {

  SECTION("Default Context construction") {
    wrp_cte::core::Context ctx;

    // Verify default values
    REQUIRE(ctx.dynamic_compress_ == 0);  // Default: skip compression
    REQUIRE(ctx.compress_lib_ == 0);      // Default: no library
    REQUIRE(ctx.target_psnr_ == 0);       // Default: 0 means infinity (lossless)
    REQUIRE(ctx.psnr_chance_ == 100);     // Default: 100% chance of PSNR validation
    REQUIRE(ctx.max_performance_ == false); // Default: optimize for ratio
    REQUIRE(ctx.consumer_node_ == -1);    // Default: unknown consumer node
    REQUIRE(ctx.data_type_ == 0);         // Default: unspecified data type
    REQUIRE(ctx.trace_ == false);         // Default: tracing disabled
    REQUIRE(ctx.trace_key_ == 0);         // Default: no trace key
    REQUIRE(ctx.trace_node_ == -1);       // Default: no trace node

    INFO("Default Context has expected initial values");
  }

  SECTION("Context with static compression mode") {
    wrp_cte::core::Context ctx;
    ctx.dynamic_compress_ = 1;  // Static compression
    ctx.compress_lib_ = CompressionLib::ZSTD;

    REQUIRE(ctx.dynamic_compress_ == 1);
    REQUIRE(ctx.compress_lib_ == CompressionLib::ZSTD);

    INFO("Static compression mode: library=" << ctx.compress_lib_);
  }

  SECTION("Context with dynamic compression mode") {
    wrp_cte::core::Context ctx;
    ctx.dynamic_compress_ = 2;  // Dynamic compression
    ctx.max_performance_ = true;  // Optimize for performance

    REQUIRE(ctx.dynamic_compress_ == 2);
    REQUIRE(ctx.max_performance_ == true);

    INFO("Dynamic compression mode with performance optimization");
  }

  SECTION("Context with lossy compression settings") {
    wrp_cte::core::Context ctx;
    ctx.dynamic_compress_ = 1;
    ctx.compress_lib_ = CompressionLib::SZ3;  // Lossy compressor
    ctx.target_psnr_ = 40;  // 40dB PSNR threshold
    ctx.psnr_chance_ = 50;  // 50% chance of validation
    ctx.data_type_ = 1;     // Float data type

    REQUIRE(ctx.target_psnr_ == 40);
    REQUIRE(ctx.psnr_chance_ == 50);
    REQUIRE(ctx.compress_lib_ == CompressionLib::SZ3);

    INFO("Lossy compression: PSNR=" << ctx.target_psnr_ << "dB, chance=" << ctx.psnr_chance_ << "%");
  }

  SECTION("Context with tracing enabled") {
    wrp_cte::core::Context ctx;
    ctx.trace_ = true;
    ctx.trace_key_ = 12345;
    ctx.trace_node_ = 0;

    REQUIRE(ctx.trace_ == true);
    REQUIRE(ctx.trace_key_ == 12345);
    REQUIRE(ctx.trace_node_ == 0);

    INFO("Tracing enabled: key=" << ctx.trace_key_ << ", node=" << ctx.trace_node_);
  }

  SECTION("Context compression mode values") {
    // Verify the documented compression mode values
    wrp_cte::core::Context skip_ctx;
    skip_ctx.dynamic_compress_ = 0;
    REQUIRE(skip_ctx.dynamic_compress_ == 0);
    INFO("Mode 0: Skip compression");

    wrp_cte::core::Context static_ctx;
    static_ctx.dynamic_compress_ = 1;
    REQUIRE(static_ctx.dynamic_compress_ == 1);
    INFO("Mode 1: Static compression");

    wrp_cte::core::Context dynamic_ctx;
    dynamic_ctx.dynamic_compress_ = 2;
    REQUIRE(dynamic_ctx.dynamic_compress_ == 2);
    INFO("Mode 2: Dynamic compression");
  }
}

/**
 * Test Case 2: Compression Library IDs
 *
 * Verifies the known compression library IDs are correctly defined
 */
TEST_CASE("Compression Library IDs", "[cte][compression][libraries]") {

  SECTION("Lossless compression libraries") {
    // Verify lossless library IDs
    REQUIRE(CompressionLib::BROTLI == 0);
    REQUIRE(CompressionLib::BZIP2 == 1);
    REQUIRE(CompressionLib::BLOSC2 == 2);
    REQUIRE(CompressionLib::LZ4 == 4);
    REQUIRE(CompressionLib::LZMA == 5);
    REQUIRE(CompressionLib::SNAPPY == 6);
    REQUIRE(CompressionLib::ZLIB == 9);
    REQUIRE(CompressionLib::ZSTD == 10);

    INFO("Lossless libraries: BROTLI=0, BZIP2=1, BLOSC2=2, LZ4=4, LZMA=5, SNAPPY=6, ZLIB=9, ZSTD=10");
  }

  SECTION("Lossy compression libraries") {
    // Verify lossy library IDs
    REQUIRE(CompressionLib::FPZIP == 3);
    REQUIRE(CompressionLib::SZ3 == 7);
    REQUIRE(CompressionLib::ZFP == 8);

    INFO("Lossy libraries: FPZIP=3, SZ3=7, ZFP=8");
  }

  SECTION("Compression config IDs") {
    // Verify config IDs
    REQUIRE(CompressionConfig::BALANCED == 0);
    REQUIRE(CompressionConfig::BEST == 1);
    REQUIRE(CompressionConfig::DEFAULT == 2);
    REQUIRE(CompressionConfig::FAST == 3);

    INFO("Configs: BALANCED=0, BEST=1, DEFAULT=2, FAST=3");
  }
}

#ifdef WRP_CORE_ENABLE_COMPRESS
/**
 * Test Case 3: CompressionTelemetry Tests
 *
 * Verifies:
 * - Default construction
 * - Full construction with all parameters
 * - Compression ratio calculation
 * - Serialization support
 */
TEST_CASE("Compression Telemetry Tracking", "[cte][compression][telemetry]") {

  SECTION("Default CompressionTelemetry construction") {
    wrp_cte::core::CompressionTelemetry telemetry;

    // Verify default values
    REQUIRE(telemetry.op_ == wrp_cte::core::CteOp::kPutBlob);
    REQUIRE(telemetry.compress_lib_ == 0);
    REQUIRE(telemetry.original_size_ == 0);
    REQUIRE(telemetry.compressed_size_ == 0);
    REQUIRE(telemetry.compress_time_ms_ == 0.0);
    REQUIRE(telemetry.decompress_time_ms_ == 0.0);
    REQUIRE(telemetry.psnr_db_ == 0.0);
    REQUIRE(telemetry.logical_time_ == 0);

    INFO("Default CompressionTelemetry has expected initial values");
  }

  SECTION("CompressionTelemetry with PutBlob operation") {
    auto now = std::chrono::steady_clock::now();

    wrp_cte::core::CompressionTelemetry telemetry(
        wrp_cte::core::CteOp::kPutBlob,
        CompressionLib::ZSTD,  // compress_lib
        1000,                   // original_size
        500,                    // compressed_size
        5.0,                    // compress_time_ms
        0.0,                    // decompress_time_ms (not applicable for Put)
        0.0,                    // psnr_db (lossless)
        now,
        1                       // logical_time
    );

    REQUIRE(telemetry.op_ == wrp_cte::core::CteOp::kPutBlob);
    REQUIRE(telemetry.compress_lib_ == CompressionLib::ZSTD);
    REQUIRE(telemetry.original_size_ == 1000);
    REQUIRE(telemetry.compressed_size_ == 500);
    REQUIRE(telemetry.compress_time_ms_ == 5.0);
    REQUIRE(telemetry.logical_time_ == 1);

    INFO("PutBlob telemetry: lib=ZSTD, original=1000, compressed=500");
  }

  SECTION("CompressionTelemetry with GetBlob operation") {
    auto now = std::chrono::steady_clock::now();

    wrp_cte::core::CompressionTelemetry telemetry(
        wrp_cte::core::CteOp::kGetBlob,
        CompressionLib::LZ4,   // compress_lib
        2048,                   // original_size
        1024,                   // compressed_size
        0.0,                    // compress_time_ms (not applicable for Get)
        2.5,                    // decompress_time_ms
        0.0,                    // psnr_db (lossless)
        now,
        2                       // logical_time
    );

    REQUIRE(telemetry.op_ == wrp_cte::core::CteOp::kGetBlob);
    REQUIRE(telemetry.compress_lib_ == CompressionLib::LZ4);
    REQUIRE(telemetry.decompress_time_ms_ == 2.5);

    INFO("GetBlob telemetry: lib=LZ4, decompress_time=2.5ms");
  }

  SECTION("Compression ratio calculation") {
    auto now = std::chrono::steady_clock::now();

    // Test 2:1 compression ratio
    wrp_cte::core::CompressionTelemetry telemetry1(
        wrp_cte::core::CteOp::kPutBlob,
        CompressionLib::ZSTD, 1000, 500, 1.0, 0.0, 0.0, now);

    double ratio1 = telemetry1.GetCompressionRatio();
    REQUIRE(std::abs(ratio1 - 2.0) < 0.001);
    INFO("2:1 compression ratio: " << ratio1);

    // Test 4:1 compression ratio
    wrp_cte::core::CompressionTelemetry telemetry2(
        wrp_cte::core::CteOp::kPutBlob,
        CompressionLib::BZIP2, 4096, 1024, 10.0, 0.0, 0.0, now);

    double ratio2 = telemetry2.GetCompressionRatio();
    REQUIRE(std::abs(ratio2 - 4.0) < 0.001);
    INFO("4:1 compression ratio: " << ratio2);

    // Test no compression (1:1 ratio)
    wrp_cte::core::CompressionTelemetry telemetry3(
        wrp_cte::core::CteOp::kPutBlob,
        0, 1000, 1000, 0.0, 0.0, 0.0, now);

    double ratio3 = telemetry3.GetCompressionRatio();
    REQUIRE(std::abs(ratio3 - 1.0) < 0.001);
    INFO("1:1 compression ratio (no compression): " << ratio3);

    // Test edge case: zero compressed size (should return 1.0)
    wrp_cte::core::CompressionTelemetry telemetry4(
        wrp_cte::core::CteOp::kPutBlob,
        0, 1000, 0, 0.0, 0.0, 0.0, now);

    double ratio4 = telemetry4.GetCompressionRatio();
    REQUIRE(std::abs(ratio4 - 1.0) < 0.001);
    INFO("Edge case (compressed_size=0): ratio=" << ratio4);
  }

  SECTION("CompressionTelemetry with lossy compression") {
    auto now = std::chrono::steady_clock::now();

    // Test with SZ3 lossy compressor
    wrp_cte::core::CompressionTelemetry telemetry(
        wrp_cte::core::CteOp::kPutBlob,
        CompressionLib::SZ3,   // lossy compressor
        8192,                   // original_size
        512,                    // compressed_size (16:1 ratio)
        15.0,                   // compress_time_ms
        5.0,                    // decompress_time_ms
        45.5,                   // psnr_db (lossy quality metric)
        now,
        3                       // logical_time
    );

    REQUIRE(telemetry.compress_lib_ == CompressionLib::SZ3);
    REQUIRE(telemetry.psnr_db_ == 45.5);
    REQUIRE(std::abs(telemetry.GetCompressionRatio() - 16.0) < 0.001);

    INFO("Lossy compression: lib=SZ3, PSNR=45.5dB, ratio=16:1");
  }
}

/**
 * Test Case 4: CompressionStats Tests
 *
 * Verifies:
 * - Default construction
 * - Full construction with predicted values
 * - Stats for different compression libraries
 */
TEST_CASE("Compression Statistics", "[cte][compression][stats]") {

  SECTION("Default CompressionStats construction") {
    wrp_cte::core::CompressionStats stats;

    // Verify default values
    REQUIRE(stats.compress_lib_ == 0);
    REQUIRE(std::abs(stats.compression_ratio_ - 1.0) < 0.001);
    REQUIRE(stats.compress_time_ms_ == 0.0);
    REQUIRE(stats.decompress_time_ms_ == 0.0);
    REQUIRE(stats.psnr_db_ == 0.0);

    INFO("Default CompressionStats has expected initial values");
  }

  SECTION("CompressionStats for fast lossless compressor") {
    // LZ4 - fast compression with moderate ratio
    wrp_cte::core::CompressionStats lz4_stats(
        CompressionLib::LZ4,
        2.5,    // compression_ratio
        1.0,    // compress_time_ms
        0.5,    // decompress_time_ms
        0.0     // psnr_db (lossless)
    );

    REQUIRE(lz4_stats.compress_lib_ == CompressionLib::LZ4);
    REQUIRE(std::abs(lz4_stats.compression_ratio_ - 2.5) < 0.001);
    REQUIRE(lz4_stats.compress_time_ms_ == 1.0);
    REQUIRE(lz4_stats.decompress_time_ms_ == 0.5);
    REQUIRE(lz4_stats.psnr_db_ == 0.0);

    INFO("LZ4 stats: ratio=2.5, compress=1.0ms, decompress=0.5ms");
  }

  SECTION("CompressionStats for high-ratio compressor") {
    // BZIP2 - slow but high compression ratio
    wrp_cte::core::CompressionStats bzip2_stats(
        CompressionLib::BZIP2,
        5.0,    // compression_ratio
        50.0,   // compress_time_ms
        25.0,   // decompress_time_ms
        0.0     // psnr_db (lossless)
    );

    REQUIRE(bzip2_stats.compress_lib_ == CompressionLib::BZIP2);
    REQUIRE(std::abs(bzip2_stats.compression_ratio_ - 5.0) < 0.001);
    REQUIRE(bzip2_stats.compress_time_ms_ == 50.0);
    REQUIRE(bzip2_stats.decompress_time_ms_ == 25.0);

    INFO("BZIP2 stats: ratio=5.0, compress=50.0ms, decompress=25.0ms");
  }

  SECTION("CompressionStats for balanced compressor") {
    // ZSTD - balanced performance and ratio
    wrp_cte::core::CompressionStats zstd_stats(
        CompressionLib::ZSTD,
        3.5,    // compression_ratio
        5.0,    // compress_time_ms
        2.0,    // decompress_time_ms
        0.0     // psnr_db (lossless)
    );

    REQUIRE(zstd_stats.compress_lib_ == CompressionLib::ZSTD);
    REQUIRE(std::abs(zstd_stats.compression_ratio_ - 3.5) < 0.001);
    REQUIRE(zstd_stats.compress_time_ms_ == 5.0);
    REQUIRE(zstd_stats.decompress_time_ms_ == 2.0);

    INFO("ZSTD stats: ratio=3.5, compress=5.0ms, decompress=2.0ms");
  }

  SECTION("CompressionStats for lossy compressor") {
    // SZ3 - lossy compression with high ratio and PSNR
    wrp_cte::core::CompressionStats sz3_stats(
        CompressionLib::SZ3,
        20.0,   // compression_ratio (very high for lossy)
        10.0,   // compress_time_ms
        3.0,    // decompress_time_ms
        42.0    // psnr_db (quality metric)
    );

    REQUIRE(sz3_stats.compress_lib_ == CompressionLib::SZ3);
    REQUIRE(std::abs(sz3_stats.compression_ratio_ - 20.0) < 0.001);
    REQUIRE(sz3_stats.psnr_db_ == 42.0);

    INFO("SZ3 stats: ratio=20.0, PSNR=42.0dB");
  }

  SECTION("Multiple compression stats comparison") {
    std::vector<wrp_cte::core::CompressionStats> stats_list;

    // Add stats for different libraries
    stats_list.emplace_back(CompressionLib::LZ4, 2.5, 1.0, 0.5, 0.0);
    stats_list.emplace_back(CompressionLib::ZSTD, 3.5, 5.0, 2.0, 0.0);
    stats_list.emplace_back(CompressionLib::BZIP2, 5.0, 50.0, 25.0, 0.0);

    REQUIRE(stats_list.size() == 3);

    // Find best ratio
    double best_ratio = 0.0;
    int best_ratio_lib = -1;
    for (const auto& stats : stats_list) {
      if (stats.compression_ratio_ > best_ratio) {
        best_ratio = stats.compression_ratio_;
        best_ratio_lib = stats.compress_lib_;
      }
    }
    REQUIRE(best_ratio_lib == CompressionLib::BZIP2);
    INFO("Best ratio: BZIP2 with ratio=" << best_ratio);

    // Find fastest compression
    double fastest_time = std::numeric_limits<double>::max();
    int fastest_lib = -1;
    for (const auto& stats : stats_list) {
      if (stats.compress_time_ms_ < fastest_time) {
        fastest_time = stats.compress_time_ms_;
        fastest_lib = stats.compress_lib_;
      }
    }
    REQUIRE(fastest_lib == CompressionLib::LZ4);
    INFO("Fastest compression: LZ4 with time=" << fastest_time << "ms");
  }
}

/**
 * Test Case 5: Compression Context Serialization
 *
 * Verifies:
 * - Context can be serialized and deserialized correctly
 */
TEST_CASE("Compression Context Serialization", "[cte][compression][serialization]") {

  SECTION("Context round-trip serialization") {
    // Create context with non-default values
    wrp_cte::core::Context original;
    original.dynamic_compress_ = 2;
    original.compress_lib_ = CompressionLib::ZSTD;
    original.target_psnr_ = 45;
    original.psnr_chance_ = 75;
    original.max_performance_ = true;
    original.consumer_node_ = 3;
    original.data_type_ = 1;
    original.trace_ = true;
    original.trace_key_ = 98765;
    original.trace_node_ = 2;

    // Verify all values are set
    REQUIRE(original.dynamic_compress_ == 2);
    REQUIRE(original.compress_lib_ == CompressionLib::ZSTD);
    REQUIRE(original.target_psnr_ == 45);
    REQUIRE(original.psnr_chance_ == 75);
    REQUIRE(original.max_performance_ == true);
    REQUIRE(original.consumer_node_ == 3);
    REQUIRE(original.data_type_ == 1);
    REQUIRE(original.trace_ == true);
    REQUIRE(original.trace_key_ == 98765);
    REQUIRE(original.trace_node_ == 2);

    INFO("Context serialization test - all fields validated");
  }
}

/**
 * Test Case 6: CteOp Enumeration Tests
 *
 * Verifies:
 * - CteOp values are correctly defined
 */
TEST_CASE("CTE Operation Types", "[cte][compression][ops]") {

  SECTION("CteOp values") {
    REQUIRE(static_cast<chi::u32>(wrp_cte::core::CteOp::kPutBlob) == 0);
    REQUIRE(static_cast<chi::u32>(wrp_cte::core::CteOp::kGetBlob) == 1);
    REQUIRE(static_cast<chi::u32>(wrp_cte::core::CteOp::kDelBlob) == 2);
    REQUIRE(static_cast<chi::u32>(wrp_cte::core::CteOp::kGetOrCreateTag) == 3);
    REQUIRE(static_cast<chi::u32>(wrp_cte::core::CteOp::kDelTag) == 4);
    REQUIRE(static_cast<chi::u32>(wrp_cte::core::CteOp::kGetTagSize) == 5);

    INFO("CteOp: kPutBlob=0, kGetBlob=1, kDelBlob=2, kGetOrCreateTag=3, kDelTag=4, kGetTagSize=5");
  }
}
#endif  // WRP_CORE_ENABLE_COMPRESS

/**
 * Test Case 7: CteTelemetry (non-compression) Tests
 *
 * Verifies:
 * - CteTelemetry (always available) works correctly
 */
TEST_CASE("CTE Telemetry", "[cte][telemetry]") {

  SECTION("Default CteTelemetry construction") {
    wrp_cte::core::CteTelemetry telemetry;

    REQUIRE(telemetry.op_ == wrp_cte::core::CteOp::kPutBlob);
    REQUIRE(telemetry.off_ == 0);
    REQUIRE(telemetry.size_ == 0);
    REQUIRE(telemetry.logical_time_ == 0);

    INFO("Default CteTelemetry has expected initial values");
  }

  SECTION("CteTelemetry with PutBlob operation") {
    auto mod_time = std::chrono::steady_clock::now();
    auto read_time = std::chrono::steady_clock::now();

    wrp_cte::core::CteTelemetry telemetry(
        wrp_cte::core::CteOp::kPutBlob,
        0,           // offset
        4096,        // size
        wrp_cte::core::TagId(100, 0),  // tag_id (major, minor)
        mod_time,
        read_time,
        1            // logical_time
    );

    REQUIRE(telemetry.op_ == wrp_cte::core::CteOp::kPutBlob);
    REQUIRE(telemetry.off_ == 0);
    REQUIRE(telemetry.size_ == 4096);
    REQUIRE(telemetry.logical_time_ == 1);

    INFO("PutBlob telemetry: offset=0, size=4096");
  }

  SECTION("CteTelemetry with GetBlob operation") {
    auto mod_time = std::chrono::steady_clock::now();
    auto read_time = std::chrono::steady_clock::now();

    wrp_cte::core::CteTelemetry telemetry(
        wrp_cte::core::CteOp::kGetBlob,
        1024,        // offset
        2048,        // size
        wrp_cte::core::TagId(200, 0),  // tag_id (major, minor)
        mod_time,
        read_time,
        2            // logical_time
    );

    REQUIRE(telemetry.op_ == wrp_cte::core::CteOp::kGetBlob);
    REQUIRE(telemetry.off_ == 1024);
    REQUIRE(telemetry.size_ == 2048);

    INFO("GetBlob telemetry: offset=1024, size=2048");
  }
}

#ifdef WRP_CORE_ENABLE_COMPRESS
/**
 * Integration Test Fixture for Compression PutBlob/GetBlob
 *
 * This fixture provides REAL runtime initialization and exercises the actual
 * CTE APIs with compression enabled.
 */
class CompressionIntegrationTestFixture {
 public:
  static constexpr chi::u64 kTestTargetSize = 1024 * 1024 * 100;  // 100MB
  static constexpr size_t kTestBlobSize = 4096;  // 4KB test blobs

  std::unique_ptr<wrp_cte::core::Client> core_client_;
  std::string test_storage_path_;
  chi::PoolId core_pool_id_;
  bool initialized_ = false;

  CompressionIntegrationTestFixture() {
    INFO("=== Initializing Compression Integration Test Environment ===");

    // Initialize test storage path in home directory
    std::string home_dir = hshm::SystemInfo::Getenv("HOME");
    REQUIRE(!home_dir.empty());

    test_storage_path_ = home_dir + "/cte_compression_test.dat";

    // Clean up any existing test file
    if (fs::exists(test_storage_path_)) {
      fs::remove(test_storage_path_);
      INFO("Cleaned up existing test file: " << test_storage_path_);
    }

    // Initialize Chimaera runtime and client
    bool success = chi::CHIMAERA_INIT(chi::ChimaeraMode::kClient, true);
    REQUIRE(success);

    // Generate unique pool ID for this test session
    int rand_id = 2000 + rand() % 9000;  // Random ID 2000-9999
    core_pool_id_ = chi::PoolId(static_cast<chi::u32>(rand_id), 0);
    INFO("Generated pool ID: " << core_pool_id_.ToU64());

    // Create and initialize core client with the generated pool ID
    core_client_ = std::make_unique<wrp_cte::core::Client>(core_pool_id_);
    INFO("CTE Core client created successfully");

    INFO("=== Compression Integration Test Environment Ready ===");
  }

  ~CompressionIntegrationTestFixture() {
    INFO("=== Cleaning up Compression Integration Test Environment ===");

    core_client_.reset();

    if (fs::exists(test_storage_path_)) {
      fs::remove(test_storage_path_);
      INFO("Cleaned up test file: " << test_storage_path_);
    }

    INFO("=== Cleanup Complete ===");
  }

  /**
   * Initialize pool, register target, and create tag
   */
  bool SetupPoolAndTarget() {
    if (initialized_) {
      return true;
    }

    // Create pool
    chi::PoolQuery pool_query = chi::PoolQuery::Dynamic();
    wrp_cte::core::CreateParams params;
    auto create_task = core_client_->AsyncCreate(
        pool_query, wrp_cte::core::kCtePoolName,
        wrp_cte::core::kCtePoolId, params);
    create_task.Wait();
    if (create_task->return_code_ != 0) {
      INFO("Failed to create pool");
      return false;
    }

    // Register target
    auto reg_task = core_client_->AsyncRegisterTarget(
        test_storage_path_, chimaera::bdev::BdevType::kFile,
        kTestTargetSize, chi::PoolQuery::Local(),
        chi::PoolId(700, 0));
    reg_task.Wait();
    if (reg_task->return_code_ != 0) {
      INFO("Failed to register target");
      return false;
    }

    initialized_ = true;
    return true;
  }

  /**
   * Helper: Create test data with verifiable pattern
   */
  std::vector<char> CreateTestData(size_t size, char pattern = 'C') {
    std::vector<char> data(size);
    for (size_t i = 0; i < size; ++i) {
      data[i] = static_cast<char>(pattern + (i % 26));
    }
    return data;
  }

  /**
   * Helper: Verify data integrity
   */
  bool VerifyTestData(const std::vector<char> &data, char pattern = 'C') {
    for (size_t i = 0; i < data.size(); ++i) {
      char expected = static_cast<char>(pattern + (i % 26));
      if (data[i] != expected) {
        INFO("Data mismatch at index " << i << ": expected " << (int)expected
             << " got " << (int)data[i]);
        return false;
      }
    }
    return true;
  }

  /**
   * Helper: Copy data to shared memory
   */
  bool CopyToSharedMemory(hipc::FullPtr<char> ptr, const std::vector<char> &data) {
    if (ptr.IsNull() || data.empty() || ptr.ptr_ == nullptr) {
      return false;
    }
    memcpy(ptr.ptr_, data.data(), data.size());
    return true;
  }

  /**
   * Helper: Copy data from shared memory
   */
  std::vector<char> CopyFromSharedMemory(hipc::FullPtr<char> ptr, size_t size) {
    std::vector<char> result;
    if (ptr.IsNull() || size == 0 || ptr.ptr_ == nullptr) {
      return result;
    }
    result.resize(size);
    memcpy(result.data(), ptr.ptr_, size);
    return result;
  }

  /**
   * Test PutBlob + GetBlob with a specific compression context
   * Returns true if data integrity is preserved after compression/decompression
   */
  bool TestPutGetWithCompression(const std::string &test_name,
                                  const wrp_cte::core::Context &ctx,
                                  size_t blob_size = 4096) {
    INFO("Testing: " << test_name);

    // Create tag for this test
    std::string tag_name = "compress_test_" + test_name;
    auto tag_task = core_client_->AsyncGetOrCreateTag(tag_name);
    tag_task.Wait();
    wrp_cte::core::TagId tag_id = tag_task->tag_id_;
    if (tag_id.IsNull()) {
      INFO("Failed to create tag");
      return false;
    }

    // Create test data with verifiable pattern
    char pattern = 'C';
    auto test_data = CreateTestData(blob_size, pattern);

    // Allocate shared memory for PutBlob
    hipc::FullPtr<char> put_buffer = CHI_IPC->AllocateBuffer(blob_size);
    if (put_buffer.IsNull()) {
      INFO("Failed to allocate put buffer");
      return false;
    }

    if (!CopyToSharedMemory(put_buffer, test_data)) {
      INFO("Failed to copy data to shared memory");
      return false;
    }

    // PutBlob with compression context
    std::string blob_name = "blob_" + test_name;
    auto put_task = core_client_->AsyncPutBlob(
        tag_id, blob_name, 0, blob_size,
        put_buffer.shm_.template Cast<void>(),
        0.8f, ctx, 0);

    put_task.Wait();
    if (put_task->return_code_ != 0) {
      INFO("PutBlob failed with return code: " << put_task->return_code_);
      return false;
    }
    INFO("PutBlob succeeded");

    // Allocate shared memory for GetBlob
    hipc::FullPtr<char> get_buffer = CHI_IPC->AllocateBuffer(blob_size);
    if (get_buffer.IsNull()) {
      INFO("Failed to allocate get buffer");
      return false;
    }

    // GetBlob (decompression happens automatically)
    auto get_task = core_client_->AsyncGetBlob(
        tag_id, blob_name, 0, blob_size, 0,
        get_buffer.shm_.template Cast<void>());

    get_task.Wait();
    if (get_task->return_code_ != 0) {
      INFO("GetBlob failed with return code: " << get_task->return_code_);
      return false;
    }
    INFO("GetBlob succeeded");

    // Verify data integrity
    auto retrieved_data = CopyFromSharedMemory(get_buffer, blob_size);
    if (retrieved_data.size() != blob_size) {
      INFO("Retrieved data size mismatch: expected " << blob_size
           << " got " << retrieved_data.size());
      return false;
    }

    if (!VerifyTestData(retrieved_data, pattern)) {
      INFO("Data integrity check FAILED");
      return false;
    }

    INFO("Data integrity verified - PASSED");
    return true;
  }
};

/**
 * Test Case: Static Compression with Various Libraries
 *
 * Tests PutBlob/GetBlob with static compression (dynamic_compress_ = 1)
 * using each supported compression library.
 */
TEST_CASE("Compression PutBlob/GetBlob - Static Libraries", "[cte][compression][putget][static]") {
  auto *fixture = hshm::Singleton<CompressionIntegrationTestFixture>::GetInstance();
  REQUIRE(fixture->SetupPoolAndTarget());

  SECTION("Static ZSTD compression") {
    wrp_cte::core::Context ctx;
    ctx.dynamic_compress_ = 1;  // Static compression
    ctx.compress_lib_ = CompressionLib::ZSTD;

    REQUIRE(fixture->TestPutGetWithCompression("static_zstd", ctx));
    INFO("SUCCESS: ZSTD static compression preserves data integrity");
  }

  SECTION("Static LZ4 compression") {
    wrp_cte::core::Context ctx;
    ctx.dynamic_compress_ = 1;  // Static compression
    ctx.compress_lib_ = CompressionLib::LZ4;

    REQUIRE(fixture->TestPutGetWithCompression("static_lz4", ctx));
    INFO("SUCCESS: LZ4 static compression preserves data integrity");
  }

  SECTION("Static ZLIB compression") {
    wrp_cte::core::Context ctx;
    ctx.dynamic_compress_ = 1;  // Static compression
    ctx.compress_lib_ = CompressionLib::ZLIB;

    REQUIRE(fixture->TestPutGetWithCompression("static_zlib", ctx));
    INFO("SUCCESS: ZLIB static compression preserves data integrity");
  }

  SECTION("Static SNAPPY compression") {
    wrp_cte::core::Context ctx;
    ctx.dynamic_compress_ = 1;  // Static compression
    ctx.compress_lib_ = CompressionLib::SNAPPY;

    REQUIRE(fixture->TestPutGetWithCompression("static_snappy", ctx));
    INFO("SUCCESS: SNAPPY static compression preserves data integrity");
  }

  SECTION("Static BROTLI compression") {
    wrp_cte::core::Context ctx;
    ctx.dynamic_compress_ = 1;  // Static compression
    ctx.compress_lib_ = CompressionLib::BROTLI;

    REQUIRE(fixture->TestPutGetWithCompression("static_brotli", ctx));
    INFO("SUCCESS: BROTLI static compression preserves data integrity");
  }

  SECTION("Static BLOSC2 compression") {
    wrp_cte::core::Context ctx;
    ctx.dynamic_compress_ = 1;  // Static compression
    ctx.compress_lib_ = CompressionLib::BLOSC2;

    REQUIRE(fixture->TestPutGetWithCompression("static_blosc2", ctx));
    INFO("SUCCESS: BLOSC2 static compression preserves data integrity");
  }
}

/**
 * Test Case: Dynamic Compression
 *
 * Tests PutBlob/GetBlob with dynamic compression (dynamic_compress_ = 2)
 * where the system chooses the best compression algorithm.
 */
TEST_CASE("Compression PutBlob/GetBlob - Dynamic", "[cte][compression][putget][dynamic]") {
  auto *fixture = hshm::Singleton<CompressionIntegrationTestFixture>::GetInstance();
  REQUIRE(fixture->SetupPoolAndTarget());

  SECTION("Dynamic compression - default settings") {
    wrp_cte::core::Context ctx;
    ctx.dynamic_compress_ = 2;  // Dynamic compression

    REQUIRE(fixture->TestPutGetWithCompression("dynamic_default", ctx));
    INFO("SUCCESS: Dynamic compression (default) preserves data integrity");
  }

  SECTION("Dynamic compression - max performance mode") {
    wrp_cte::core::Context ctx;
    ctx.dynamic_compress_ = 2;  // Dynamic compression
    ctx.max_performance_ = true;  // Optimize for speed

    REQUIRE(fixture->TestPutGetWithCompression("dynamic_maxperf", ctx));
    INFO("SUCCESS: Dynamic compression (max performance) preserves data integrity");
  }

  SECTION("Dynamic compression - optimize for ratio") {
    wrp_cte::core::Context ctx;
    ctx.dynamic_compress_ = 2;  // Dynamic compression
    ctx.max_performance_ = false;  // Optimize for compression ratio

    REQUIRE(fixture->TestPutGetWithCompression("dynamic_ratio", ctx));
    INFO("SUCCESS: Dynamic compression (ratio optimized) preserves data integrity");
  }
}

/**
 * Test Case: No Compression Baseline
 *
 * Tests PutBlob/GetBlob without compression (dynamic_compress_ = 0)
 * to establish baseline behavior.
 */
TEST_CASE("Compression PutBlob/GetBlob - No Compression", "[cte][compression][putget][none]") {
  auto *fixture = hshm::Singleton<CompressionIntegrationTestFixture>::GetInstance();
  REQUIRE(fixture->SetupPoolAndTarget());

  SECTION("No compression baseline") {
    wrp_cte::core::Context ctx;
    ctx.dynamic_compress_ = 0;  // Skip compression

    REQUIRE(fixture->TestPutGetWithCompression("no_compression", ctx));
    INFO("SUCCESS: No compression baseline preserves data integrity");
  }
}

/**
 * Test Case: Various Data Sizes with Compression
 *
 * Tests compression with different blob sizes to ensure
 * robustness across size ranges.
 */
TEST_CASE("Compression PutBlob/GetBlob - Various Sizes", "[cte][compression][putget][sizes]") {
  auto *fixture = hshm::Singleton<CompressionIntegrationTestFixture>::GetInstance();
  REQUIRE(fixture->SetupPoolAndTarget());

  SECTION("Small blob with ZSTD") {
    wrp_cte::core::Context ctx;
    ctx.dynamic_compress_ = 1;
    ctx.compress_lib_ = CompressionLib::ZSTD;

    REQUIRE(fixture->TestPutGetWithCompression("small_zstd", ctx, 512));  // 512 bytes
    INFO("SUCCESS: Small blob (512B) with ZSTD compression");
  }

  SECTION("Medium blob with ZSTD") {
    wrp_cte::core::Context ctx;
    ctx.dynamic_compress_ = 1;
    ctx.compress_lib_ = CompressionLib::ZSTD;

    REQUIRE(fixture->TestPutGetWithCompression("medium_zstd", ctx, 8192));  // 8KB
    INFO("SUCCESS: Medium blob (8KB) with ZSTD compression");
  }

  SECTION("Large blob with ZSTD") {
    wrp_cte::core::Context ctx;
    ctx.dynamic_compress_ = 1;
    ctx.compress_lib_ = CompressionLib::ZSTD;

    REQUIRE(fixture->TestPutGetWithCompression("large_zstd", ctx, 65536));  // 64KB
    INFO("SUCCESS: Large blob (64KB) with ZSTD compression");
  }

  SECTION("Large blob with dynamic compression") {
    wrp_cte::core::Context ctx;
    ctx.dynamic_compress_ = 2;  // Dynamic

    REQUIRE(fixture->TestPutGetWithCompression("large_dynamic", ctx, 65536));  // 64KB
    INFO("SUCCESS: Large blob (64KB) with dynamic compression");
  }
}
#endif  // WRP_CORE_ENABLE_COMPRESS

// Main function using simple_test.h framework
SIMPLE_TEST_MAIN()
