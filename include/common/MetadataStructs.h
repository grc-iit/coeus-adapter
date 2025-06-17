//
// Created by jaime on 9/29/2023.
//

#ifndef COEUS_INCLUDE_COMMON_METADATASTRUCTS_H_
#define COEUS_INCLUDE_COMMON_METADATASTRUCTS_H_

enum semantics{
  MIN,
  MAX
};

constexpr std::string_view semantics_to_string(semantics s) {
  switch (s) {
    case semantics::MIN: return "min";
    case semantics::MAX: return "max";
      // handle more enum values as needed
    default: return "unknown";
  }
}

struct derivedSemantics {
  float min_value;
  float max_value;

 public:
  derivedSemantics(float min, float max) : min_value(min), max_value(max) { }
  derivedSemantics() = default;

    template <class Archive>
    void serialize(Archive &ar) {
        ar(min_value, max_value);
    }
};

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
  bool derived;
  bool constantShape;
  std::string dataType;

  VariableMetadata() = default;
  VariableMetadata(const std::string &name, const std::vector<size_t> &shape,
                   const std::vector<size_t> &start,
                   const std::vector<size_t> &count, bool constantShape, bool derived,
                   const std::string &dataType)
      : name(name), shape(shape), start(start), count(count),
        constantShape(constantShape), derived(derived), dataType(dataType) {}

  static std::string serializeVector(const std::vector<size_t>& vec) {
    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i) {
      oss << vec[i];
      if (i != vec.size() - 1) {
        oss << ",";
      }
    }
    return oss.str();
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


  bool operator==(const VariableMetadata& other) const {
    return name == other.name &&
        shape == other.shape &&
        start == other.start &&
        count == other.count &&
        constantShape == other.constantShape;
  }

  template <class Archive>
  void serialize(Archive &ar) {
    ar(name, shape, start, count, constantShape, derived, dataType);
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
