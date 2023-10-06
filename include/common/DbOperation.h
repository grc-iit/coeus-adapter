//
// Created by jaime on 10/5/2023.
//

#ifndef COEUS_INCLUDE_COMMON_DBOPERATION_H_
#define COEUS_INCLUDE_COMMON_DBOPERATION_H_


enum class OperationType {
  InsertData,
  UpdateSteps
};

class DbOperation {
 private:
  OperationType type;
  int step;
  int rank;
  VariableMetadata metadata;
  std::string name;
  BlobInfo blobInfo;  // Assuming BlobInfo is a defined structure
  std::string uid;
  int currentStep;

 public:
  // Constructor for InsertData type
  DbOperation(int _step, int _rank, VariableMetadata _metadata, std::string _name, BlobInfo _blobInfo)
      : type(OperationType::InsertData), step(_step), rank(_rank), metadata(std::move(_metadata)),
      name(std::move(_name)), blobInfo(std::move(_blobInfo)) {}

  // Constructor for UpdateSteps type
  DbOperation(std::string _uid, int _currentStep)
      : type(OperationType::UpdateSteps), uid(std::move(_uid)), currentStep(_currentStep) {}

  friend class DbQueueWorker;  // so that DbQueueWorker can access private members of DbOperation
};


#endif //COEUS_INCLUDE_COMMON_DBOPERATION_H_
