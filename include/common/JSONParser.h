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

#ifndef COEUS_INCLUDE_COMMON_JSONPARSER_H_
#define COEUS_INCLUDE_COMMON_JSONPARSER_H_

#include <iostream>
#include <string>
#include <unordered_map>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>

#include "ErrorCodes.h"

// Use 'using' to give the complex type a simpler name
using JSONMap = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;

class JSONParser {
 private:
  rapidjson::Document document;

 public:
  JSONParser(const std::string& path) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) {
      throw ErrorException(NO_JSON);
    }

    char readBuffer[65536];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    document.ParseStream(is);
    fclose(fp);
  }

  bool isValid() const {
    return !document.HasParseError() && document.IsObject();
  }

  JSONMap parse() {
    JSONMap resultMap;

    if (!isValid()) {
      throw ErrorException(BAD_JSON);
    }

    for (auto& var : document.GetObject()) {
      std::string variableName = var.name.GetString();

      if (!var.value.IsObject()) continue;

      std::unordered_map<std::string, std::string> operatorMap;
      for (auto& op : var.value.GetObject()) {
        std::string operatorName = op.name.GetString();
        std::string operatorFunction = op.value.GetString();

        operatorMap[operatorName] = operatorFunction;
      }
      resultMap[variableName] = operatorMap;
    }

    return resultMap;
  }
};

#endif //COEUS_INCLUDE_COMMON_JSONPARSER_H_
