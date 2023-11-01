

#include "GlobalVariable.h"
#include <unordered_map>
#include <vector>

int PutCount = 0; // Define and initialize the global variable
int GetCount = 0; // Define and initialize another global variable
std::unordered_map<std::string, std::vector<int>> PutMap; // Define and initialize an unordered_map
std::unordered_map<std::string, std::vector<int>> GetMap; // Define and initialize another unordered_map

