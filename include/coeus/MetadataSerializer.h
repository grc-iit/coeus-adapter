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

#ifndef COEUS_INCLUDE_COEUS_METADATASERIALIZER_H_
#define COEUS_INCLUDE_COEUS_METADATASERIALIZER_H_

#include <hermes/hermes_types.h>
#include "common/MetadataStructs.h"

#include <vector>
#include <string>

#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <adios2/core/Variable.h>
#include <adios2/cxx11/Variable.h>

class MetadataSerializer{
 public:
  static std::string SerializeBlobInfo(BlobInfo blob_info) {
    std::stringstream ss;
    {
      cereal::BinaryOutputArchive oarchive(ss);
      oarchive(blob_info);
    }
    return ss.str();
  }

  static BlobInfo DeserializeBlobInfo(const hermes::Blob &blob) {
    BlobInfo blob_info;
    std::stringstream ss;
    {
      ss.write(reinterpret_cast<char*>(blob.data()), blob.size());
      cereal::BinaryInputArchive iarchive(ss);
      iarchive(blob_info);
    }
    return blob_info;
  }

    static std::string SerializeMetadata(VariableMetadata variableMetadata) {
      std::stringstream ss;
      {
        cereal::BinaryOutputArchive oarchive(ss);
        oarchive(variableMetadata);
      }
      return ss.str();
    }

    template<typename T>
    static std::string SerializeMetadata(adios2::core::Variable<T> variable) {
        VariableMetadata variableMetadata(variable);
        return SerializeMetadata(variableMetadata);
    }

    template<typename T>
    static std::string SerializeMetadata(const adios2::Variable<T> &variable) {
        const VariableMetadata variableMetadata(variable);
        return SerializeMetadata(variableMetadata);
    }

    static VariableMetadata DeserializeMetadata(const hermes::Blob &blob) {
      VariableMetadata variableMetadata;
      std::stringstream ss;
      {
          ss.write(reinterpret_cast<char*>(blob.data()), blob.size());
          cereal::BinaryInputArchive iarchive(ss);
          iarchive(variableMetadata);
      }
      return variableMetadata;
    }
};


#endif  // COEUS_INCLUDE_COEUS_METADATASERIALIZER_H_
