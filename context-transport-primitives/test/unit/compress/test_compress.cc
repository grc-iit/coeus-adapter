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

#include "basic_test.h"
#include "hermes_shm/compress/compress_factory.h"
#include <utility>

TEST_CASE("TestCompress") {
  std::string raw = "Hello, World!";
  std::vector<char> compressed(1024);
  std::vector<char> decompressed(1024);

  PAGE_DIVIDE("BZIP2") {
    hshm::Bzip2 bzip;
    size_t cmpr_size = 1024, raw_size = 1024;
    bzip.Compress(compressed.data(), cmpr_size,
                  raw.data(), raw.size());
    bzip.Decompress(decompressed.data(), raw_size,
                    compressed.data(), cmpr_size);
    REQUIRE(raw == std::string(decompressed.data(), raw_size));
  }

  PAGE_DIVIDE("LZO") {
    hshm::Lzo lzo;
    size_t cmpr_size = 1024, raw_size = 1024;
    lzo.Compress(compressed.data(), cmpr_size,
                 raw.data(), raw.size());
    lzo.Decompress(decompressed.data(), raw_size,
                   compressed.data(), cmpr_size);
    REQUIRE(raw == std::string(decompressed.data(), raw_size));
  }

  PAGE_DIVIDE("Zstd") {
    hshm::Zstd zstd;
    size_t cmpr_size = 1024, raw_size = 1024;
    zstd.Compress(compressed.data(), cmpr_size,
                  raw.data(), raw.size());
    zstd.Decompress(decompressed.data(), raw_size,
                    compressed.data(), cmpr_size);
    REQUIRE(raw == std::string(decompressed.data(), raw_size));
  }

  PAGE_DIVIDE("LZ4") {
    hshm::Lz4 lz4;
    size_t cmpr_size = 1024, raw_size = 1024;
    lz4.Compress(compressed.data(), cmpr_size,
                 raw.data(), raw.size());
    lz4.Decompress(decompressed.data(), raw_size,
                   compressed.data(), cmpr_size);
    REQUIRE(raw == std::string(decompressed.data(), raw_size));
  }

  PAGE_DIVIDE("Zlib") {
    hshm::Zlib zlib;
    size_t cmpr_size = 1024, raw_size = 1024;
    zlib.Compress(compressed.data(), cmpr_size,
                  raw.data(), raw.size());
    zlib.Decompress(decompressed.data(), raw_size,
                    compressed.data(), cmpr_size);
    REQUIRE(raw == std::string(decompressed.data(), raw_size));
  }

  PAGE_DIVIDE("Lzma") {
    hshm::Lzma lzma;
    size_t cmpr_size = 1024, raw_size = 1024;
    lzma.Compress(compressed.data(), cmpr_size,
                  raw.data(), raw.size());
    lzma.Decompress(decompressed.data(), raw_size,
                    compressed.data(), cmpr_size);
    REQUIRE(raw == std::string(decompressed.data(), raw_size));
  }

  PAGE_DIVIDE("Brotli") {
    hshm::Brotli brotli;
    size_t cmpr_size = 1024, raw_size = 1024;
    brotli.Compress(compressed.data(), cmpr_size,
                    raw.data(), raw.size());
    brotli.Decompress(decompressed.data(), raw_size,
                      compressed.data(), cmpr_size);
    REQUIRE(raw == std::string(decompressed.data(), raw_size));
  }

  PAGE_DIVIDE("Snappy") {
    hshm::Snappy snappy;
    size_t cmpr_size = 1024, raw_size = 1024;
    snappy.Compress(compressed.data(), cmpr_size,
                    raw.data(), raw.size());
    snappy.Decompress(decompressed.data(), raw_size,
                      compressed.data(), cmpr_size);
    REQUIRE(raw == std::string(decompressed.data(), raw_size));
  }

  PAGE_DIVIDE("Blosc2") {
    hshm::Blosc blosc;
    size_t cmpr_size = 1024, raw_size = 1024;
    blosc.Compress(compressed.data(), cmpr_size,
                   raw.data(), raw.size());
    blosc.Decompress(decompressed.data(), raw_size,
                     compressed.data(), cmpr_size);
    REQUIRE(raw == std::string(decompressed.data(), raw_size));
  }
}
