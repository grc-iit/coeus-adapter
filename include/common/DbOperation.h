
//
// Created by jaime on 10/5/2023.
//

#ifndef COEUS_INCLUDE_COMMON_DBOPERATION_H_
#define COEUS_INCLUDE_COMMON_DBOPERATION_H_

#include <common/MetadataStructs.h>

#include <utility>

enum class OperationType {
  InsertData,
  UpdateSteps,
  InsertDerivedData,
  CheckVariable,
};

class DbOperation {
 public:
  OperationType type;
  int step;
  int rank;
  VariableMetadata metadata;
  std::string name;
  BlobInfo blobInfo;  // Assuming BlobInfo is a defined structure
  std::string uid;
  int currentStep;
  derivedSemantics derived_semantics;

  DbOperation() = default;

  // Constructor for InsertData type
  DbOperation(int _step, int _rank, VariableMetadata _metadata, std::string  _name,
              BlobInfo _blobInfo, derivedSemantics _derived_semantics)
      : type(OperationType::InsertDerivedData), step(_step), rank(_rank), metadata(std::move(_metadata)),
      name(std::move(_name)), blobInfo(std::move(_blobInfo)), derived_semantics(std::move(_derived_semantics)) {}

  DbOperation(int _step, int _rank, VariableMetadata _metadata, std::string  _name,
              BlobInfo _blobInfo)
      : type(OperationType::InsertData), step(_step), rank(_rank), metadata(std::move(_metadata)),
        name(std::move(_name)), blobInfo(std::move(_blobInfo)) {}

  // Constructor for UpdateSteps type
  DbOperation(const std::string& _uid, int _currentStep)
      : type(OperationType::UpdateSteps), uid(std::move(_uid)), currentStep(_currentStep) {}

  //constructor for check the name
  DbOperation(int _step, int _rank, std::string _name)
      : type(OperationType::CheckVariable), step(_step), rank(_rank), name(std::move(_name)) {}

  template <class Archive>
  void serialize(Archive &ar) {
    ar(type, step, rank, metadata, name, blobInfo, uid, currentStep, derived_semantics);
  }
};


#endif //COEUS_INCLUDE_COMMON_DBOPERATION_H_
