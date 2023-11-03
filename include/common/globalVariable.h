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
#ifndef COEUS_ADAPTER_GLOBALVARIABLE_H
#define COEUS_ADAPTER_GLOBALVARIABLE_H

#include <unordered_map>
#include <vector>
#include <string>




class GlobalVariable {
public:
    int PutCount;
    int GetCount;
    std::unordered_map<std::string, std::vector<int>> PutMap;
    std::unordered_map<std::string, std::vector<int>> GetMap;

    GlobalVariable() : PutCount(0), GetCount(0) {
        // You can initialize the maps or other variables here if needed.
    }
    void insertPut (std::string str) {
        PutMap[str].push_back(PutCount);
        PutCount++;
    }
    void insertGet (std::string str) {
        GetMap[str].push_back(GetCount);
        GetCount++;
    }
    std::string PutMapToString() {
        std::stringstream result;
        for (const auto& entry : PutMap) {
            result << "Key: " << entry.first << " Order: ";
            for (int order : entry.second) {
                result << order << ' ';
            }
            result << '\n';
        }
        return result.str();
    }

    std::string GetMapToString() {
        std::stringstream result;
        for (const auto& entry : GetMap) {
            result << "Key: " << entry.first << " Order: ";
            for (int order : entry.second) {
                result << order << ' ';
            }
            result << '\n';
        }
        return result.str();
    }

    // You can add member functions to manipulate or access the data if necessary.
};

#endif // COEUS_ADAPTER_GLOBALVARIABLE_H