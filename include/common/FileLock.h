//
// Created by jaime on 10/3/2023.
//

#ifndef COEUS_INCLUDE_COMMON_FILELOCK_H_
#define COEUS_INCLUDE_COMMON_FILELOCK_H_

#include <iostream>
#include <fcntl.h>
#include <sys/file.h>

class FileLock {
 private:
  int fd;
  std::string lockFileName;

 public:
  FileLock(const std::string& lockFileName) : lockFileName(lockFileName) {
    fd = open(lockFileName.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
      std::cerr << "Error opening lock file." << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  void lock() {
    if (flock(fd, LOCK_EX) == -1) {
      std::cerr << "Error locking file." << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  void unlock() {
    if (flock(fd, LOCK_UN) == -1) {
      std::cerr << "Error unlocking file." << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  ~FileLock() {
    close(fd);
  }
};

#endif //COEUS_INCLUDE_COMMON_FILELOCK_H_
