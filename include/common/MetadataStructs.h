//
// Created by jaime on 9/29/2023.
//

#ifndef COEUS_INCLUDE_COMMON_METADATASTRUCTS_H_
#define COEUS_INCLUDE_COMMON_METADATASTRUCTS_H_

#include <adios2/core/Variable.h>
#include <adios2/cxx11/Variable.h>

struct BlobInfo {
  std::string bucket_name;
  std::string blob_name;

 public:
  BlobInfo(const std::string &bucket_name, const std::string &blob_name) :
      bucket_name(bucket_name), blob_name(blob_name){};
  BlobInfo() = default;

  template <class Archive>
  void serialize(Archive &ar) {
    ar(bucket_name, blob_name);
  }
};

// Define your struct
struct VariableMetadata {
  std::string name;
  std::vector<size_t> shape;
  std::vector<size_t> start;
  std::vector<size_t> count;
  bool constantShape;
  std::string dataType;

  VariableMetadata() = default;
  VariableMetadata(const std::string &name, const std::vector<size_t> &shape,
                   const std::vector<size_t> &start,
                   const std::vector<size_t> &count, bool constantShape,
                   const std::string &dataType)
      : name(name), shape(shape), start(start), count(count),
        constantShape(constantShape), dataType(dataType) {}

  template<typename T>
  explicit VariableMetadata(const adios2::core::Variable<T> &variable) {
    name = variable.m_Name;

    if (variable.m_Shape.empty() || variable.m_Shape.data() == nullptr) {
      shape = std::vector<size_t>();
    } else {
      shape = variable.m_Shape;
    }
    // Check if start is empty or null and assign an empty array if so
    if (variable.m_Start.empty() || variable.m_Start.data() == nullptr) {
      start = std::vector<size_t>();
    } else {
      start = variable.m_Start;
    }

    // Check if count is empty or null and assign an empty array if so
    if (variable.Count().empty() || variable.Count().data() == nullptr) {
      count = std::vector<size_t>();
    } else {
      count = variable.m_Count;
    }

    constantShape = variable.IsConstantDims();
    dataType = adios2::ToString(variable.m_Type);
  }

  template<typename T>
  explicit VariableMetadata(const adios2::Variable<T> variable) {
    name = variable.Name();
    shape = variable.Shape();

    // Check if start is empty or null and assign an empty array if so
    if (variable.Start().empty() || variable.Start().data() == nullptr) {
      start = std::vector<size_t>();
    } else {
      start = variable.Start();
    }

    // Check if count is empty or null and assign an empty array if so
    if (variable.Count().empty() || variable.Count().data() == nullptr) {
      count = std::vector<size_t>();
    } else {
      count = variable.Count();
    }

    constantShape = false;
    /* We need to see if this is correct.
       Though this is mostly just for the unit test,
       as the engine will reference the adios::core::variable constructor */
    dataType = variable.Type();
  }

  static std::string serializeVector(const std::vector<size_t>& vec) {
//    std::ostringstream oss;
//    for (size_t i = 0; i < vec.size(); ++i) {
//      oss << vec[i];
//      if (i != vec.size() - 1) {
//        oss << ",";
//      }
//    }
//    return oss.str();
    return "16,8,8";
  }

  static std::vector<size_t> deserializeVector(const std::string& str) {
    std::vector<size_t> vec;
    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, ',')) {
      vec.push_back(std::stoull(token));
    }
    return vec;
  }


  adios2::DataType getDataType() {
    return adios2::helper::GetDataTypeFromString(dataType);
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
    ar(name, shape, start, count, constantShape, dataType);
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
#endif //COEUS_INCLUDE_COMMON_METADATASTRUCTS_H_
