#ifndef COEUS_ADAPTER_GLOBALVARIABLE_H
#define COEUS_ADAPTER_GLOBALVARIABLE_H

#include <unordered_map>
#include <vector>
#include <string>




class GlobalVariables {
public:
    int PutCount;
    int GetCount;
    std::unordered_map<std::string, std::vector<int>> PutMap;
    std::unordered_map<std::string, std::vector<int>> GetMap;

    GlobalVariables() : PutCount(0), GetCount(0) {
        // You can initialize the maps or other variables here if needed.
    }

    // You can add member functions to manipulate or access the data if necessary.
};

#endif // COEUS_ADAPTER_GLOBALVARIABLE_H