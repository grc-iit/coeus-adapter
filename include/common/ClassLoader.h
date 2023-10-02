/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Coeus-adapter. The full Coeus-adapter copyright      *
 * notice, including terms governing use, modification, and redistribution,  *
 * is contained in the COPYING file, which can be found at the top directory.*
 * If you do not have access to the file, you may request a copy             *
 * from scslab@iit.edu.                                                      *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef COEUS_INCLUDE_COMMON_CLASSLOADER_H
#define COEUS_INCLUDE_COMMON_CLASSLOADER_H

#include <dlfcn.h>
#include <boost/filesystem.hpp>
#include <filesystem>

class ClassLoader {
public:
    ClassLoader(){ m_job_handler = nullptr; }

    void* loadLib(std::string lib){
      auto lib_obj = dlopen(lib.c_str(), RTLD_NOW | RTLD_GLOBAL);
      if (!lib_obj) {
        fprintf(stderr, "%s\n", dlerror());
      }
      dlerror(); // clear error code
      return lib_obj;
    }

    template<typename T>
    std::shared_ptr<T> LoadClass(std::string path, uint32_t class_id_) {
        /**
         * TODO: please test this logic. we want JOB_PATH to be a dir and we will check for the symbol in each so present in the directory.
         * Also please calculate the cost of finding symbol in such a case.
         */
        using namespace boost::filesystem;
        auto files = std::filesystem::recursive_directory_iterator(path);
        for (auto const & entry : files){
          const std::string file_path = entry.path().string();
          if (file_path.substr(file_path.size() - 3) == ".so"){
            auto tensorflow = loadLib("libtensorflow.so");
            auto mpi = loadLib("libmpi.so");
            auto rpc = loadLib("librpc.so");
            auto boost = loadLib("libboost_filesystem.so");
            auto sentinel = loadLib("libsentinel.so");
            auto rhea = loadLib("librhea.so");
            auto m_job_handler = loadLib(file_path.c_str());
            std::string job_symbol = "create_job_" + std::to_string(class_id_);
            std::shared_ptr<T> (*create_job_fun)();
            create_job_fun = (std::shared_ptr<T> (*)())dlsym(m_job_handler, job_symbol.c_str());
            const char *dlsym_error = NULL;
            if ((dlsym_error = dlerror()) != NULL){
                continue;
            } else {
                // create Job instance
                return create_job_fun();
            }
            }
        }
        throw ErrorException(NOT_FOUND_CLASS);

    }

private:
    void* m_job_handler;
};
#endif //COEUS_INCLUDE_COMMON_CLASSLOADER_H
