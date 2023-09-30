//
// Created by jaime on 9/13/2023.
//

#ifndef COEUS_INCLUDE_COEUS_CONTAINERMANAGER_H_
#define COEUS_INCLUDE_COEUS_CONTAINERMANAGER_H_
#include "Container.h"

using YAMLMap = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;

class ContainerManager{
    private:
    YAMLMap yamlMap;
    std::unordered_map<std::string, coeus::Container> containers;

    public:
    ContainerManager(const YAMLMap &map) {
      yamlMap = map;
      for (auto iter = yamlMap.begin(); iter != yamlMap.end(); iter++) {
        registerContainer(iter->first);
      }
    }

    bool registerContainer(const std::string &derivedName) {
        if (containers.find(derivedName) != containers.end()) {
          return false;
        }
        auto container = coeus::Container(derivedName, yamlMap[derivedName]);
        containers[derivedName] = container;
        return true;
    }

    bool registerVariable(const std::string &variableName, void *vals) {
      bool found = false;
      for (auto iter = containers.begin(); iter != containers.end(); iter++) {
        found |= iter->second.registerVariable(variableName, vals);
      }
      return found;
    }

    bool checkAndExecute(bool force = false) {
      bool found = false;
      for (auto iter = containers.begin(); iter != containers.end(); iter++) {
        found |= iter->second.checkAndExecute();
      }
      return found;
    }
};
#endif //COEUS_INCLUDE_COEUS_CONTAINERMANAGER_H_
