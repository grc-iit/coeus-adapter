//
// Created by linux on 10/20/23.
//

#ifndef COEUS_ADAPTER_GLOBALVARIABLE_H
#define COEUS_ADAPTER_GLOBALVARIABLE_H
#include <unordered_map>
#include <vector>

extern int PutCount;
extern int GetCount;
extern std::unordered_map<std::string, std::vector<int>> PutMap;
extern  std::unordered_map<std::string, std::vector<int>> GetMap;
#endif //COEUS_ADAPTER_GLOBALVARIABLE_H