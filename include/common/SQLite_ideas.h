//
// Created by jaime on 9/29/2023.
//

#ifndef COEUS_INCLUDE_COMMON_SQLITE_IDEAS_H_
#define COEUS_INCLUDE_COMMON_SQLITE_IDEAS_H_

// IO. Data distribution
bool createMetadataTable() {
  std::string createTableSQL = "CREATE TABLE IF NOT EXISTS Metadata("
                               "id INTEGER PRIMARY KEY,"
                               "start INTEGER NOT NULL,"
                               "count INTEGER NOT NULL,"
                               "file_name TEXT NOT NULL,"
                               "step INTEGER NOT NULL);";  // Added step column
  return execute(createTableSQL);
}

bool insertMetadata(int start, int count, const std::string& file_name, int step) {
  const char* insertDataSQL = "INSERT INTO Metadata (start, count, file_name, step) VALUES (?, ?, ?, ?);";
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2(db, insertDataSQL, -1, &stmt, 0);
  sqlite3_bind_int(stmt, 1, start);
  sqlite3_bind_int(stmt, 2, count);
  sqlite3_bind_text(stmt, 3, file_name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 4, step);  // Bind step value
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

std::vector<std::string> queryblobs(int start_position, int end_position) {
  const char* query = "SELECT blob_name FROM Metadata WHERE "
                      "(start >= ? AND start < ?) "
                      "OR (start + count > ? AND start + count <= ?) "
                      "OR (start <= ? AND start + count >= ?);";
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2(db, query, -1, &stmt, 0);
  sqlite3_bind_int(stmt, 1, start_position);
  sqlite3_bind_int(stmt, 2, end_position);
  sqlite3_bind_int(stmt, 3, start_position);
  sqlite3_bind_int(stmt, 4, end_position);
  sqlite3_bind_int(stmt, 5, start_position);
  sqlite3_bind_int(stmt, 6, end_position);
  std::vector<std::string> blobs;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    blobs.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
  }
  sqlite3_finalize(stmt);
  return blobs;
}

std::vector<std::pair<std::string, std::pair<int, int>>> queryBlobsWithOffset(int start_position, int end_position) {
  const char* query = "SELECT file_name, start, count FROM Metadata WHERE "
                      "(start >= ? AND start < ?) "
                      "OR (start + count > ? AND start + count <= ?) "
                      "OR (start <= ? AND start + count >= ?);";
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2(db, query, -1, &stmt, 0);
  sqlite3_bind_int(stmt, 1, start_position);
  sqlite3_bind_int(stmt, 2, end_position);
  sqlite3_bind_int(stmt, 3, start_position);
  sqlite3_bind_int(stmt, 4, end_position);
  sqlite3_bind_int(stmt, 5, start_position);
  sqlite3_bind_int(stmt, 6, end_position);

  std::vector<std::pair<std::string, std::pair<int, int>>> results;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::string file = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    int fileStart = sqlite3_column_int(stmt, 1);
    int fileCount = sqlite3_column_int(stmt, 2);

    int readStart = std::max(start_position - fileStart, 0);
    int readEnd = std::min(readStart + (end_position - start_position), fileCount);

    results.push_back({file, {readStart, readEnd}});
    start_position += (readEnd - readStart);
  }
  sqlite3_finalize(stmt);
  return results;
}

//Admin Global Metadata
bool createGlobalTable() {
  std::string createTableSQL = "CREATE TABLE IF NOT EXISTS GlobalTable("
                               "id INTEGER PRIMARY KEY,"
                               "app_name TEXT NOT NULL,"
                               "variable_name TEXT NOT NULL,"
                               "derived BOOLEAN NOT NULL,"
                               "num_processes INTEGER NOT NULL,"
                               "constant_shape BOOLEAN NOT NULL,"
                               "data_type TEXT NOT NULL,"
                               "dimensions TEXT NOT NULL);";  // Storing dimensions as comma-separated string
  return execute(createTableSQL);
}

bool insertGlobalData(const std::string& app_name, const std::string& variable_name, bool derived,
                      int num_processes, bool constant_shape, const std::string& data_type,const std::vector<size_t>& dimensions) {
  std::string dimensionsStr = "";
  for (const auto& dim : dimensions) {
    dimensionsStr += std::to_string(dim) + ",";
  }
  dimensionsStr.pop_back();  // Remove the trailing comma

  const char* insertDataSQL = "INSERT INTO GlobalTable (app_name, variable_name, derived, num_processes, constant_shape, data_type, dimensions) VALUES (?, ?, ?, ?, ?, ?);";
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2(db, insertDataSQL, -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, app_name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, variable_name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 3, derived);
  sqlite3_bind_int(stmt, 4, num_processes);
  sqlite3_bind_int(stmt, 5, constant_shape);
  sqlite3_bind_text(stmt, 6, data_type.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 7, dimensionsStr.c_str(), -1, SQLITE_STATIC);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

int queryNumberOfApps() {
  const char* query = "SELECT COUNT(DISTINCT app_name) FROM GlobalTable;";
  int count = 0;
  execute(query, &count, [](void* data, int argc, char** argv, char** azColName) -> int {
    int* countPtr = reinterpret_cast<int*>(data);
    if (argc > 0 && argv[0]) {
      *countPtr = std::stoi(argv[0]);
    }
    return 0;
  });
  return count;
}

std::vector<std::string> queryVariablesByApp(const std::string& app_name) {
  const char* query = "SELECT DISTINCT variable_name FROM GlobalTable WHERE app_name = ?;";
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2(db, query, -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, app_name.c_str(), -1, SQLITE_STATIC);
  std::vector<std::string> variables;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    variables.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
  }
  sqlite3_finalize(stmt);
  return variables;
}

VariableInfo queryInfoByVariableName(const std::string& variable_name) {
  const char* query = "SELECT app_name, derived, num_processes, constant_shape, data_type, dimensions "
                      "FROM GlobalTable WHERE variable_name = ? LIMIT 1;";
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2(db, query, -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, variable_name.c_str(), -1, SQLITE_STATIC);
  VariableInfo info;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    info.app_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    info.derived = sqlite3_column_int(stmt, 1);
    info.num_processes = sqlite3_column_int(stmt, 2);
    info.constant_shape = sqlite3_column_int(stmt, 3);
    info.data_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    std::string dimensionsStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    std::istringstream iss(dimensionsStr);
    std::string token;
    while (std::getline(iss, token, ',')) {
      info.dimensions.push_back(std::stoull(token));
    }
  }
  sqlite3_finalize(stmt);
  return info;
}

//Hierarchy: Query blobs by step
std::vector<std::string> queryBlobsByStep(int step) {
  const char* query = "SELECT file_name FROM Metadata WHERE step = ?;";
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2(db, query, -1, &stmt, 0);
  sqlite3_bind_int(stmt, 1, step);
  std::vector<std::string> blobs;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    blobs.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
  }
  sqlite3_finalize(stmt);
  return blobs;
}

//Derived Variables: Previous step


// Experimental: Gloabl Min-Max from local Min-Max
bool createSegmentDataTable() {
  std::string typeStr = getTypeString();
  std::string createTableSQL = "CREATE TABLE IF NOT EXISTS SegmentData("
                               "id INTEGER PRIMARY KEY,"
                               "min_value " + typeStr + " NOT NULL,"
                                                        "max_value " + typeStr + " NOT NULL,"
                                                                                 "blob_name TEXT NOT NULL);";
  return execute(createTableSQL);
}

bool insertSegmentData(T min_value, T max_value, const std::string& blob_name) {
  const char* insertDataSQL = "INSERT INTO SegmentData (min_value, max_value, blob_name) VALUES (?, ?, ?);";
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2(db, insertDataSQL, -1, &stmt, 0);
  if constexpr (std::is_same_v<T, int>) {
    sqlite3_bind_int(stmt, 1, min_value);
    sqlite3_bind_int(stmt, 2, max_value);
  } else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
    sqlite3_bind_double(stmt, 1, min_value);
    sqlite3_bind_double(stmt, 2, max_value);
  }
  sqlite3_bind_text(stmt, 3, blob_name.c_str(), -1, SQLITE_STATIC);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

std::pair<T, T> queryGlobalMinMax() {
  const char* query = "SELECT MIN(min_value) AS global_min, MAX(max_value) AS global_max FROM SegmentData;";
  std::pair<T, T> minMax;
  execute(query, &minMax, minMaxCallback);
  return minMax;
}

#endif //COEUS_INCLUDE_COMMON_SQLITE_IDEAS_H_
