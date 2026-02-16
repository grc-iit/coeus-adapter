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

// Dynamically checked to see which are the real APIs and which are intercepted
bool posix_intercepted = true;

#include "posix_api.h"

#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>

#include <filesystem>

#include "chimaera/chimaera.h"
#include "adapter/filesystem/filesystem.h"
#include "hermes_shm/util/logging.h"
#include "hermes_shm/util/singleton.h"
#include "posix_fs_api.h"

namespace wrp::cae {
// Define global pointer variables in source file
HSHM_DEFINE_GLOBAL_PTR_VAR_CC(PosixApi, g_posix_api);
HSHM_DEFINE_GLOBAL_PTR_VAR_CC(PosixFs, g_posix_fs);

/** Used for compatability with older kernel versions */
int fxstat_to_fstat(int fd, struct stat *stbuf) {
#ifdef _STAT_VER
  return WRP_CTE_POSIX_API->__fxstat(_STAT_VER, fd, stbuf);
#else
  (void)fd;
  (void)stbuf;
  return -1;
#endif
}
} // namespace wrp::cae


using wrp::cae::AdapterStat;
using wrp::cae::File;
using wrp::cae::FsIoOptions;
using wrp::cae::IoStatus;
using wrp::cae::MetadataManager;
using wrp::cae::SeekMode;


extern "C" {

// static __attribute__((constructor(101))) void init_posix(void) {
//   WRP_CTE_POSIX_API;
//   WRP_CTE_POSIX_FS;
//   TRANSPARENT_HERMES();;
// }

/**
 * POSIX
 */
int WRP_CTE_DECL(open)(const char *path, int flags, ...) {
  int mode = 0;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (flags & O_CREAT || flags & O_TMPFILE) {
    va_list arg;
    va_start(arg, flags);
    mode = va_arg(arg, int);
    va_end(arg);
  }
  if (real_api->IsInterceptorLoaded() && fs_api->IsPathTracked(path)) {
    HLOG(kDebug,
          "Intercept open for filename: {}"
          " and mode: {}"
          " is tracked.",
          path, flags);
    AdapterStat stat;
    stat.flags_ = flags;
    stat.st_mode_ = mode;
    auto f = fs_api->Open(stat, path);
    return f.hermes_fd_;
  }

  int fd = -1;
  if (flags & O_CREAT || flags & O_TMPFILE) {
    fd = real_api->open(path, flags, mode);
  } else {
    fd = real_api->open(path, flags);
  }
  return fd;
}

int WRP_CTE_DECL(open64)(const char *path, int flags, ...) {
  int mode = 0;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (flags & O_CREAT || flags & O_TMPFILE) {
    va_list arg;
    va_start(arg, flags);
    mode = va_arg(arg, int);
    va_end(arg);
  }
  if (real_api->IsInterceptorLoaded() && fs_api->IsPathTracked(path)) {
    HLOG(kDebug,
          "Intercept open64 for filename: {}"
          " and mode: {}"
          " is tracked.",
          path, flags);
    AdapterStat stat;
    stat.flags_ = flags;
    stat.st_mode_ = mode;
    return fs_api->Open(stat, path).hermes_fd_;
  }
  int fd = -1;
  if (flags & O_CREAT || flags & O_TMPFILE) {
    fd = real_api->open64(path, flags, mode);
  } else {
    fd = real_api->open64(path, flags);
  }
  return fd;
}

int WRP_CTE_DECL(__open_2)(const char *path, int oflag) {
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (real_api->IsInterceptorLoaded() && fs_api->IsPathTracked(path)) {
    HLOG(kDebug,
          "Intercept __open_2 for filename: {}"
          " and mode: {}"
          " is tracked.",
          path, oflag);
    AdapterStat stat;
    stat.flags_ = oflag;
    stat.st_mode_ = 0;
    return fs_api->Open(stat, path).hermes_fd_;
  }
  return real_api->__open_2(path, oflag);
}

int WRP_CTE_DECL(creat)(const char *path, mode_t mode) {
  std::string path_str(path);
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (real_api->IsInterceptorLoaded() && fs_api->IsPathTracked(path)) {
    HLOG(kDebug,
          "Intercept creat for filename: {}"
          " and mode: {}"
          " is tracked.",
          path, mode);
    AdapterStat stat;
    stat.flags_ = O_CREAT;
    stat.st_mode_ = mode;
    return fs_api->Open(stat, path).hermes_fd_;
  }
  return real_api->creat(path, mode);
}

int WRP_CTE_DECL(creat64)(const char *path, mode_t mode) {
  std::string path_str(path);
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (real_api->IsInterceptorLoaded() && fs_api->IsPathTracked(path)) {
    HLOG(kDebug,
          "Intercept creat64 for filename: {}"
          " and mode: {}"
          " is tracked.",
          path, mode);
    AdapterStat stat;
    stat.flags_ = O_CREAT;
    stat.st_mode_ = mode;
    return fs_api->Open(stat, path).hermes_fd_;
  }
  return real_api->creat64(path, mode);
}

ssize_t WRP_CTE_DECL(read)(int fd, void *buf, size_t count) {
  bool stat_exists;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsFdTracked(fd)) {
    HLOG(kDebug, "Intercept read.");
    File f;
    f.hermes_fd_ = fd;
    IoStatus io_status;
    size_t ret = fs_api->Read(f, stat_exists, buf, count, io_status);
    if (stat_exists)
      return ret;
  }
  return real_api->read(fd, buf, count);
}

ssize_t WRP_CTE_DECL(write)(int fd, const void *buf, size_t count) {
  bool stat_exists;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsFdTracked(fd)) {
    HLOG(kDebug, "Intercept write.");
    File f;
    f.hermes_fd_ = fd;
    IoStatus io_status;
    size_t ret = fs_api->Write(f, stat_exists, buf, count, io_status);
    if (stat_exists)
      return ret;
  }
  return real_api->write(fd, buf, count);
}

ssize_t WRP_CTE_DECL(pread)(int fd, void *buf, size_t count, off_t offset) {
  bool stat_exists;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsFdTracked(fd)) {
    HLOG(kDebug, "Intercept pread.");
    File f;
    f.hermes_fd_ = fd;
    IoStatus io_status;
    size_t ret = fs_api->Read(f, stat_exists, buf, offset, count, io_status);
    if (stat_exists)
      return ret;
  }
  return real_api->pread(fd, buf, count, offset);
}

ssize_t WRP_CTE_DECL(pwrite)(int fd, const void *buf, size_t count,
                            off_t offset) {
  bool stat_exists;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsFdTracked(fd)) {
    File f;
    f.hermes_fd_ = fd;
    IoStatus io_status;
    HLOG(kDebug, "Intercept pwrite.");
    size_t ret = fs_api->Write(f, stat_exists, buf, offset, count, io_status);
    if (stat_exists)
      return ret;
  }
  return real_api->pwrite(fd, buf, count, offset);
}

ssize_t WRP_CTE_DECL(pread64)(int fd, void *buf, size_t count, off64_t offset) {
  bool stat_exists;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsFdTracked(fd)) {
    File f;
    f.hermes_fd_ = fd;
    IoStatus io_status;
    HLOG(kDebug, "Intercept pread64.");
    size_t ret = fs_api->Read(f, stat_exists, buf, offset, count, io_status);
    if (stat_exists)
      return ret;
  }
  return real_api->pread64(fd, buf, count, offset);
}

ssize_t WRP_CTE_DECL(pwrite64)(int fd, const void *buf, size_t count,
                              off64_t offset) {
  bool stat_exists;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsFdTracked(fd)) {
    File f;
    f.hermes_fd_ = fd;
    IoStatus io_status;
    HLOG(kDebug, "Intercept pwrite64.");
    size_t ret = fs_api->Write(f, stat_exists, buf, offset, count, io_status);
    if (stat_exists)
      return ret;
  }
  return real_api->pwrite64(fd, buf, count, offset);
}

off_t WRP_CTE_DECL(lseek)(int fd, off_t offset, int whence) {
  bool stat_exists;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsFdTracked(fd)) {
    File f;
    f.hermes_fd_ = fd;
    HLOG(kDebug, "Intercept lseek offset: {} whence: {}.", offset, whence);
    return fs_api->Seek(f, stat_exists, static_cast<SeekMode>(whence), offset);
  }
  return real_api->lseek(fd, offset, whence);
}

off64_t WRP_CTE_DECL(lseek64)(int fd, off64_t offset, int whence) {
  bool stat_exists;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsFdTracked(fd)) {
    File f;
    f.hermes_fd_ = fd;
    HLOG(kDebug, "Intercept lseek64 offset: {} whence: {}.", offset, whence);
    return fs_api->Seek(f, stat_exists, static_cast<SeekMode>(whence), offset);
  }
  return real_api->lseek64(fd, offset, whence);
}

int WRP_CTE_DECL(__fxstat)(int __ver, int fd, struct stat *buf) {
  int result = 0;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsFdTracked(fd)) {
    HLOG(kDebug, "Intercepted __fxstat.");
    File f;
    f.hermes_fd_ = fd;
    result = fs_api->Stat(f, buf);
  } else {
    result = real_api->__fxstat(__ver, fd, buf);
  }
  return result;
}

int WRP_CTE_DECL(__fxstatat)(int __ver, int __fildes, const char *__filename,
                            struct stat *__stat_buf, int __flag) {
  int result = 0;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsPathTracked(__filename)) {
    HLOG(kDebug, "Intercepted __fxstatat.");
    result = fs_api->Stat(__filename, __stat_buf);
  } else {
    result =
        real_api->__fxstatat(__ver, __fildes, __filename, __stat_buf, __flag);
  }
  return result;
}

int WRP_CTE_DECL(__xstat)(int __ver, const char *__filename,
                         struct stat *__stat_buf) {
  int result = 0;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsPathTracked(__filename)) {
    // HLOG(kDebug, "Intercepted __xstat for file {}.", __filename);
    result = fs_api->Stat(__filename, __stat_buf);
  } else {
    result = real_api->__xstat(__ver, __filename, __stat_buf);
  }
  return result;
}

int WRP_CTE_DECL(__lxstat)(int __ver, const char *__filename,
                          struct stat *__stat_buf) {
  int result = 0;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsPathTracked(__filename)) {
    HLOG(kDebug, "Intercepted __lxstat.");
    result = fs_api->Stat(__filename, __stat_buf);
  } else {
    result = real_api->__lxstat(__ver, __filename, __stat_buf);
  }
  return result;
}

int WRP_CTE_DECL(fstat)(int fd, struct stat *buf) {
  int result = 0;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsFdTracked(fd)) {
    HLOG(kDebug, "Intercepted fstat.");
    File f;
    f.hermes_fd_ = fd;
    result = fs_api->Stat(f, buf);
  } else {
    result = real_api->fstat(fd, buf);
  }
  return result;
}

int WRP_CTE_DECL(stat)(const char *pathname, struct stat *buf) {
  int result = 0;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsPathTracked(pathname)) {
    HLOG(kDebug, "Intercepted stat.");
    result = fs_api->Stat(pathname, buf);
  } else {
    result = real_api->stat(pathname, buf);
  }
  return result;
}

int WRP_CTE_DECL(__fxstat64)(int __ver, int fd, struct stat64 *buf) {
  int result = 0;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsFdTracked(fd)) {
    HLOG(kDebug, "Intercepted __fxstat.");
    File f;
    f.hermes_fd_ = fd;
    result = fs_api->Stat(f, buf);
  } else {
    result = real_api->__fxstat64(__ver, fd, buf);
  }
  return result;
}

int WRP_CTE_DECL(__fxstatat64)(int __ver, int __fildes, const char *__filename,
                              struct stat64 *__stat_buf, int __flag) {
  int result = 0;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsPathTracked(__filename)) {
    HLOG(kDebug, "Intercepted __fxstatat.");
    result = fs_api->Stat(__filename, __stat_buf);
  } else {
    result =
        real_api->__fxstatat64(__ver, __fildes, __filename, __stat_buf, __flag);
  }
  return result;
}

int WRP_CTE_DECL(__xstat64)(int __ver, const char *__filename,
                           struct stat64 *__stat_buf) {
  int result = 0;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsPathTracked(__filename)) {
    HLOG(kDebug, "Intercepted __xstat.");
    result = fs_api->Stat(__filename, __stat_buf);
  } else {
    result = real_api->__xstat64(__ver, __filename, __stat_buf);
  }
  return result;
}

int WRP_CTE_DECL(__lxstat64)(int __ver, const char *__filename,
                            struct stat64 *__stat_buf) {
  int result = 0;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsPathTracked(__filename)) {
    HLOG(kDebug, "Intercepted __lxstat.");
    result = fs_api->Stat(__filename, __stat_buf);
  } else {
    result = real_api->__lxstat64(__ver, __filename, __stat_buf);
  }
  return result;
}

int WRP_CTE_DECL(fstat64)(int fd, struct stat64 *buf) {
  int result = 0;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsFdTracked(fd)) {
    HLOG(kDebug, "Intercepted fstat.");
    File f;
    f.hermes_fd_ = fd;
    result = fs_api->Stat(f, buf);
  } else {
    result = real_api->fstat64(fd, buf);
  }
  return result;
}

int WRP_CTE_DECL(stat64)(const char *pathname, struct stat64 *buf) {
  int result = 0;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsPathTracked(pathname)) {
    HLOG(kDebug, "Intercepted stat.");
    result = fs_api->Stat(pathname, buf);
  } else {
    result = real_api->stat64(pathname, buf);
  }
  return result;
}

int WRP_CTE_DECL(fsync)(int fd) {
  bool stat_exists;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsFdTracked(fd)) {
    File f;
    f.hermes_fd_ = fd;
    HLOG(kDebug, "Intercepted fsync.");
    return fs_api->Sync(f, stat_exists);
  }
  return real_api->fsync(fd);
}

int WRP_CTE_DECL(ftruncate)(int fd, off_t length) {
  bool stat_exists;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsFdTracked(fd)) {
    File f;
    f.hermes_fd_ = fd;
    HLOG(kDebug, "Intercepted ftruncate.");
    return fs_api->Truncate(f, stat_exists, length);
  }
  return real_api->ftruncate(fd, length);
}

int WRP_CTE_DECL(ftruncate64)(int fd, off64_t length) {
  bool stat_exists;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsFdTracked(fd)) {
    File f;
    f.hermes_fd_ = fd;
    HLOG(kDebug, "Intercepted ftruncate.");
    return fs_api->Truncate(f, stat_exists, length);
  }
  return real_api->ftruncate64(fd, length);
}

int WRP_CTE_DECL(close)(int fd) {
  bool stat_exists;
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsFdTracked(fd)) {
    HLOG(kDebug, "Intercepted close({}).", fd);
    File f;
    f.hermes_fd_ = fd;
    return fs_api->Close(f, stat_exists);
  }
  return real_api->close(fd);
}

int WRP_CTE_DECL(flock)(int fd, int operation) {
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsFdTracked(fd)) {
    HLOG(kDebug, "Intercepted flock({}).", fd);
    // TODO(llogan): implement?
    return 0;
  }
  return real_api->flock(fd, operation);
}

int WRP_CTE_DECL(remove)(const char *pathname) {
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsPathTracked(pathname)) {
    HLOG(kDebug, "Intercepted remove({})", pathname);
    return fs_api->Remove(pathname);
  }
  return real_api->remove(pathname);
}

int WRP_CTE_DECL(unlink)(const char *pathname) {
  auto real_api = WRP_CTE_POSIX_API;
  auto fs_api = WRP_CTE_POSIX_FS;
  if (fs_api->IsPathTracked(pathname)) {
    HLOG(kDebug, "Intercepted unlink({})", pathname);
    return fs_api->Remove(pathname);
  }
  return real_api->unlink(pathname);
}

} // extern C
