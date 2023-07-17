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

#ifndef COEUS_INCLUDE_COEUS_METADATA_SERIALIZER_H_
#define COEUS_INCLUDE_COEUS_METADATA_SERIALIZER_H_

#include <adios2/core/Variable.h>
#include "cereal/archives/binary.hpp"
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <hermes_types.h>
#include <adios2/cxx11/Variable.h>

// Define your struct
struct VariableMetadata {
  std::string name;
  std::vector<size_t> shape;
  std::vector<size_t> start;
  std::vector<size_t> count;
  bool constantShape;

  VariableMetadata() = default;

  template<typename T>
  explicit VariableMetadata(adios2::core::Variable<T> &variable){
    name = variable.m_Name;
    shape = variable.Shape();
    start = variable.m_Start;
    count = variable.Count();
    constantShape = variable.IsConstantDims();
  }

  template<typename T>
  VariableMetadata(adios2::Variable<T> variable) {
    name = variable.Name();
    shape = variable.Shape();
    start = variable.Start();
    count = variable.Count();
    constantShape = false; /* We need to see if this is correct.
                           *  Though this is mostly just for the unit test,
                           *  as the engine will reference the adios::core::variable constructor */
  }

  bool operator==(const VariableMetadata& other) const {
    return name == other.name &&
           shape == other.shape &&
           start == other.start &&
           count == other.count &&
           constantShape == other.constantShape;
  }

  template<typename T>
  bool operator==(const adios2::Variable<T>& other) const {
    return name == other.Name() &&
        shape == other.Shape() &&
        start == other.Start() &&
        count == other.Count() &&
        !constantShape;
  }

  template<typename T>
  bool operator==(const adios2::core::Variable<T>& other) const {
    return name == other.m_Name &&
        shape == other.Shape() &&
        start == other.m_Start &&
        count == other.Count() &&
        constantShape == other.IsConstantDims();
  }

  template <class Archive>
  void serialize(Archive &ar) {
    ar(name, shape, start, count, constantShape);
  }
};

std::ostream& operator<<(std::ostream &out, const VariableMetadata &data) {
  out << "Name: " << data.name << "\n";
  out << "Shape: ";
  for (const auto &s : data.shape) {
    out << s << " ";
  }
  out << "\nStart: ";
  for (const auto &s : data.start) {
    out << s << " ";
  }
  out << "\nCount: ";
  for (const auto &c : data.count) {
    out << c << " ";
  }
  out << "\nConstant Shape: " << (data.constantShape ? "True" : "False");
  return out;
}

class MetadataSerializer{
    public:

    static std::string SerializeMetadata(VariableMetadata variableMetadata){
      std::stringstream ss;
      {
        cereal::BinaryOutputArchive oarchive(ss);
        oarchive(variableMetadata);
      }
      return ss.str();
    }

    template<typename T>
    static std::string SerializeMetadata(adios2::core::Variable<T> variable){
      VariableMetadata variableMetadata(variable);
      return SerializeMetadata(variableMetadata);
    }

  template<typename T>
  static std::string SerializeMetadata(adios2::Variable<T> &variable){
    VariableMetadata variableMetadata(variable);
    return SerializeMetadata(variableMetadata);
  }

    static VariableMetadata DeserializeMetadata(hermes::Blob &blob){
      VariableMetadata variableMetadata;
      std::stringstream ss;
      {
        ss.write((char*)blob.data(), blob.size());
        cereal::BinaryInputArchive iarchive(ss);
        iarchive(variableMetadata);
      }
      return variableMetadata;
    }
};

#endif //COEUS_INCLUDE_COEUS_METADATA_SERIALIZER_H_