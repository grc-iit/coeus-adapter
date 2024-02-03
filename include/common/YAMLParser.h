/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
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

#ifndef COEUS_INCLUDE_COMMON_YAMLPARSER_H_
#define COEUS_INCLUDE_COMMON_YAMLPARSER_H_

#include <iostream>
#include <string>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

#include "ErrorCodes.h"

// Use 'using' to give the complex type a simpler name
using YAMLMap = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;

class YAMLParser {
 private:
  YAML::Node node;

 public:
  YAMLParser(const std::string& path) {
    try {
      node = YAML::LoadFile(path);
    } catch (const YAML::Exception& e) {
      throw ErrorException(NO_YAML);
    }
  }

  bool isValid() const {
    return !node.IsNull() && node.IsMap();
  }

  YAMLMap parse() {
    YAMLMap resultMap;

    if (!isValid()) {
      std::cout<<"Invalid YAML file"<<std::endl;
      throw ErrorException(BAD_YAML);
    }

    for (const auto& var : node) {
      std::string variableName = var.first.as<std::string>();

      if (!var.second.IsMap()) continue;

      std::unordered_map<std::string, std::string> operatorMap;
      for (const auto& op : var.second) {
        std::string operatorName = op.first.as<std::string>();
        std::string operatorFunction = op.second.as<std::string>();

        operatorMap[operatorName] = operatorFunction;
      }
      resultMap[variableName] = operatorMap;
    }

    return resultMap;
  }
};

#endif // COEUS_INCLUDE_COMMON_YAMLPARSER_H_
