//
// Created by jaime on 9/13/2023.
//

#ifndef COEUS_INCLUDE_COMMON_CONTAINER_H_
#define COEUS_INCLUDE_COMMON_CONTAINER_H_

struct AdiosData {
  std::string variableName;
  void *values;

  AdiosData(std::string name, void *vals) {
    variableName = name;
    values = vals;
  }

  AdiosData() = default;
};

namespace coeus {
class Container {
 private:
  std::string _variableName;
  std::string _operatorName;
  std::unordered_map<std::string, AdiosData> _inputs;
  std::vector<std::string> _inputNames;
  bool executing = false;

 public:

  bool registerVariable(const std::string &name, void *vals) {
    if (std::find(_inputNames.begin(), _inputNames.end(), name) == _inputNames.end()) {
      return false;
    }
    auto data = AdiosData(name, vals);
    _inputs[name] = data;
    return true;
  }

  bool checkAndExecute(bool force = false) {
    if (_inputs.size() == _inputNames.size()) {

      //TODO: execute

      _inputs.clear();
      return true;
    }
    return false;
  }

  Container(const std::string &name, const std::string& operatorName, std::vector<std::string> inputs) {
    _variableName = name;
    _inputNames = inputs;
    _operatorName = operatorName;
  }

  Container(const std::string &name, std::unordered_map<std::string, std::string> inputs) {
    _variableName = name;
    std::vector<std::string> inputNames;

    for (auto iter = inputs.begin(); iter != inputs.end(); iter++) {
      std::string elementName = iter->first;
      if(elementName.find("operatorName") != std::string::npos){
        _operatorName = iter->second;
        continue;
      }
      if(elementName.find("internalVariable") != std::string::npos){
        inputNames.push_back(iter->second);
        continue;
      }
      inputNames.push_back(iter->first);
    }
    _inputNames = inputNames;
  }

  Container() = default;
};
} // namespace coeus
#endif //COEUS_INCLUDE_COMMON_CONTAINER_H_
