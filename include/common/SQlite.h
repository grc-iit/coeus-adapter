//
// Created by jaime on 8/21/2023.
//

#ifndef COEUS_INCLUDE_COMMON_SQLITE_H_
#define COEUS_INCLUDE_COMMON_SQLITE_H_

#include <iostream>
#include <sqlite3.h>
#include <string>
#include <vector>
#include <utility>
#include <type_traits>
#include <filesystem>
#include "common/MetadataStructs.h"
#include "comms/Bucket.h"

class SQLiteWrapper {
 private:
  sqlite3* db;
  bool open;
  std::string dbName;
  bool deleteOnDestruction;

  bool execute(const std::string& sql, void* data = nullptr, int (*callbackFunc)(void*, int, char**, char**) = nullptr) {
    char* errMsg = 0;
    int rc = sqlite3_exec(db, sql.c_str(), callbackFunc, data, &errMsg);
    if (rc != SQLITE_OK) {
      std::cerr << "SQL error: " << errMsg << std::endl;
      sqlite3_free(errMsg);
      return false;
    }
    return true;
  }

  void clearDatabase() {
    std::filesystem::remove(dbName);
  }

 public:
  SQLiteWrapper(const std::string& dbName, bool deleteOnDestruction = false)
      : dbName(dbName), deleteOnDestruction(deleteOnDestruction) {
    if (sqlite3_open(dbName.c_str(), &db)) {
      std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
      sqlite3_close(db);
      open=false;
    }
    open=true;
    createAppsTable();
    createBlobLocationsTable();
    createVariableMetadataTable();
  }

  ~SQLiteWrapper() {
    sqlite3_close(db);
    if (deleteOnDestruction) {
      clearDatabase();
    }
    open=false;
  }

  bool isOpen(){
    return open;
  }

  /***************************************
   * Total Steps
   ****************************************/
  bool createAppsTable() {
    const std::string createTableSQL = "CREATE TABLE IF NOT EXISTS Apps ("
                                       "appName TEXT PRIMARY KEY,"
                                       "TotalSteps INTEGER);";
    return execute(createTableSQL);
  }

  void UpdateTotalSteps(const std::string& appName, int step) {
    sqlite3_stmt* stmt;
    const std::string insertOrUpdateSQL = "INSERT OR REPLACE INTO Apps (appName, TotalSteps) VALUES (?, ?);";
    sqlite3_prepare_v2(db, insertOrUpdateSQL.c_str(), -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, appName.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, step);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }

  int GetTotalSteps(const std::string& appName) {
    sqlite3_stmt* stmt;
    const std::string selectSQL = "SELECT TotalSteps FROM Apps WHERE appName = ?;";
    sqlite3_prepare_v2(db, selectSQL.c_str(), -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, appName.c_str(), -1, SQLITE_STATIC);
    int totalSteps = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      totalSteps = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return totalSteps;
  }

  std::vector<std::string> GetAppList() {
    sqlite3_stmt* stmt;
    const std::string selectSQL = "SELECT appName FROM Apps;";
    sqlite3_prepare_v2(db, selectSQL.c_str(), -1, &stmt, 0);
    std::vector<std::string> appList;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      appList.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    return appList;
  }

  /***************************************
 * Data Location
 ****************************************/
  bool createBlobLocationsTable() {
    const std::string createTableSQL = "CREATE TABLE IF NOT EXISTS BlobLocations ("
                                       "step INTEGER,"
                                       "mpi_rank INTEGER,"
                                       "name TEXT,"
                                       "bucket_name TEXT,"
                                       "blob_name TEXT,"
                                       "PRIMARY KEY (step, mpi_rank, name));";
    return execute(createTableSQL);
  }

  void InsertBlobLocation(int step, int mpi_rank, const std::string& varName, const BlobInfo& blobInfo) {
    sqlite3_stmt* stmt;
    const std::string insertOrUpdateSQL = "INSERT OR REPLACE INTO BlobLocations (step, mpi_rank, name, bucket_name, blob_name) VALUES (?, ?, ?, ?, ?);";
    sqlite3_prepare_v2(db, insertOrUpdateSQL.c_str(), -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, step);
    sqlite3_bind_int(stmt, 2, mpi_rank);
    sqlite3_bind_text(stmt, 3, varName.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, blobInfo.bucket_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, blobInfo.blob_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }

  BlobInfo GetBlobLocation(int step, int mpi_rank, const std::string& name) {
    sqlite3_stmt* stmt;
    const std::string selectSQL = "SELECT bucket_name, blob_name FROM BlobLocations WHERE step = ? AND mpi_rank = ? AND name = ?;";
    sqlite3_prepare_v2(db, selectSQL.c_str(), -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, step);
    sqlite3_bind_int(stmt, 2, mpi_rank);
    sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_STATIC);
    BlobInfo blobInfo;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      blobInfo.bucket_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
      blobInfo.blob_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    }
    sqlite3_finalize(stmt);
    return blobInfo;
  }

  /***************************************
* MetaData Location
****************************************/

  bool createVariableMetadataTable() {
    const std::string createTableSQL = "CREATE TABLE IF NOT EXISTS VariableMetadataTable ("
                                       "step INTEGER,"
                                       "mpi_rank INTEGER,"
                                       "name TEXT,"
                                       "shape TEXT,"
                                       "start TEXT,"
                                       "count TEXT,"
                                       "constantShape BOOLEAN,"
                                       "dataType TEXT,"
                                       "PRIMARY KEY (step, mpi_rank, name));";
    return execute(createTableSQL);
  }

  void InsertVariableMetadata(int step, int mpi_rank, const VariableMetadata& metadata) {
    sqlite3_stmt* stmt;
    const std::string insertOrUpdateSQL = "INSERT OR REPLACE INTO VariableMetadataTable (step, mpi_rank, name, shape, start, count, constantShape, dataType) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_prepare_v2(db, insertOrUpdateSQL.c_str(), -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, step);
    sqlite3_bind_int(stmt, 2, mpi_rank);
    sqlite3_bind_text(stmt, 3, metadata.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, VariableMetadata::serializeVector(metadata.shape).c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, VariableMetadata::serializeVector(metadata.start).c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, VariableMetadata::serializeVector(metadata.count).c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, metadata.constantShape);
    sqlite3_bind_text(stmt, 8, metadata.dataType.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }

  VariableMetadata GetVariableMetadata(int step, int mpi_rank, const std::string& name) {
    sqlite3_stmt* stmt;
    const std::string selectSQL = "SELECT shape, start, count, constantShape, dataType FROM VariableMetadataTable WHERE step = ? AND mpi_rank = ? AND name = ?;";
    sqlite3_prepare_v2(db, selectSQL.c_str(), -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, step);
    sqlite3_bind_int(stmt, 2, mpi_rank);
    sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_STATIC);
    VariableMetadata metadata;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      metadata.name = name;
      metadata.shape = VariableMetadata::deserializeVector(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
      metadata.start = VariableMetadata::deserializeVector(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
      metadata.count = VariableMetadata::deserializeVector(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
      metadata.constantShape = sqlite3_column_int(stmt, 3);
      metadata.dataType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    }
    sqlite3_finalize(stmt);
    return metadata;
  }


};

#endif //COEUS_INCLUDE_COMMON_SQLITE_H_