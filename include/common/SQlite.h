//
// Created by jaime on 8/21/2023.
//

#ifndef COEUS_INCLUDE_COMMON_SQLITE_H_
#define COEUS_INCLUDE_COMMON_SQLITE_H_

#include <iostream>
#include <sqlite3.h>
#include <string>
#include <unistd.h>
#include <vector>
#include <utility>
#include <type_traits>
#include <filesystem>

#include "common/MetadataStructs.h"

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
      sqlite3_free(errMsg);
      return false;
    }

    return true;
  }

  void clearDatabase() {
    std::filesystem::remove(dbName);
  }

 public:
  std::string getName(){
    return dbName;
  }
  SQLiteWrapper(const std::string& dbName, bool deleteOnDestruction = false)
      : dbName(dbName), deleteOnDestruction(deleteOnDestruction) {
    if (sqlite3_open(dbName.c_str(), &db)) {
      std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
      sqlite3_close(db);
      open=false;
    }
    open=true;
  }

  void createTables(){
    createAppsTable();
    createBlobLocationsTable();
    createVariableMetadataTable();
    CreateDerivedTargetsTable();
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

    bool FindVariable(int step, int mpi_rank, const std::string& varName) {
        sqlite3_stmt* stmt;
        const std::string findVariable = "SELECT COUNT(*) FROM BlobLocations WHERE step = ? AND mpi_rank = ? AND name = ?;";
        sqlite3_prepare_v2(db, findVariable.c_str(), -1, &stmt, 0);
        // Prepare the SQL statement

        // Bind the parameters
        sqlite3_bind_int(stmt, 1, step);
        sqlite3_bind_int(stmt, 2, mpi_rank);
        sqlite3_bind_text(stmt, 3, varName.c_str(), -1, SQLITE_STATIC);

        // Execute the statement and check if the variable exists
        bool exists = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int count = sqlite3_column_int(stmt, 0);
            exists = (count > 0);
        }

        // Clean up and finalize the statement
        sqlite3_finalize(stmt);
        return exists;
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

  std::vector<BlobInfo> getAllBlobs(int step, int mpi_rank) {
    if (step < 0) {
      return {}; // Return an empty vector if step is invalid
    }

    std::vector<BlobInfo> allBlobInfos;
    sqlite3_stmt* stmt;
    const std::string selectSQL = "SELECT bucket_name, blob_name FROM BlobLocations WHERE step = ? AND mpi_rank = ?;";

    if (sqlite3_prepare_v2(db, selectSQL.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
      // Handle error...
    }

    sqlite3_bind_int(stmt, 1, step);
    sqlite3_bind_int(stmt, 2, mpi_rank);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
      BlobInfo blobInfo;
      blobInfo.bucket_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
      blobInfo.blob_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
      allBlobInfos.push_back(blobInfo);
    }

    sqlite3_finalize(stmt);
    return allBlobInfos;
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
                                       "derived BOOLEAN,"
                                       "dataType TEXT,"
                                       "PRIMARY KEY (step, mpi_rank, name));";
    return execute(createTableSQL);
  }

  void InsertVariableMetadata(int step, int mpi_rank, const VariableMetadata& metadata) {

    sqlite3_stmt* stmt;
    const std::string insertOrUpdateSQL = "INSERT INTO VariableMetadataTable (step, mpi_rank, name, shape, start, count, constantShape, derived, dataType) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
    auto shape = VariableMetadata::serializeVector(metadata.shape);
    auto start = VariableMetadata::serializeVector(metadata.start);
    auto count = VariableMetadata::serializeVector(metadata.count);

    sqlite3_prepare_v2(db, insertOrUpdateSQL.c_str(), -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, step);
    sqlite3_bind_int(stmt, 2, mpi_rank);
    sqlite3_bind_text(stmt, 3, metadata.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, shape.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, start.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, count.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, metadata.constantShape);
    sqlite3_bind_int(stmt, 8, metadata.derived);
    sqlite3_bind_text(stmt, 9, metadata.dataType.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }

  VariableMetadata GetVariableMetadata(int step, int mpi_rank, const std::string& name) {
    sqlite3_stmt* stmt;
    const std::string selectSQL = "SELECT shape, start, count, constantShape, derived, dataType FROM VariableMetadataTable WHERE step = ? AND mpi_rank = ? AND name = ?;";
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
      metadata.derived = sqlite3_column_int(stmt, 4);
      metadata.dataType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    }
    sqlite3_finalize(stmt);
    return metadata;
  }

  std::vector<VariableMetadata> GetAllVariableMetadata(int step, int mpi_rank) {
    sqlite3_stmt* stmt;
    const std::string selectSQL = "SELECT name, shape, start, count, constantShape, derived, dataType FROM VariableMetadataTable WHERE step = ? AND mpi_rank = ?;";
    sqlite3_prepare_v2(db, selectSQL.c_str(), -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, step);
    sqlite3_bind_int(stmt, 2, mpi_rank);

    std::vector<VariableMetadata> allMetadata;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
      VariableMetadata metadata;

      metadata.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
      metadata.shape = VariableMetadata::deserializeVector(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
      metadata.start = VariableMetadata::deserializeVector(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
      metadata.count = VariableMetadata::deserializeVector(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
      metadata.constantShape = sqlite3_column_int(stmt, 4);
      metadata.derived = sqlite3_column_int(stmt, 5);
      metadata.dataType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));

      allMetadata.push_back(metadata);
    }

    sqlite3_finalize(stmt);
    return allMetadata;
  }


  bool CreateDerivedTargetsTable() {
    const std::string sqlCreateTable = "CREATE TABLE IF NOT EXISTS derived_targets ("
                                       "step INTEGER,"
                                       "variable TEXT,"
                                       "operation TEXT,"
                                       "blob_name TEXT,"
                                       "bucket_name TEXT,"
                                       "value REAL,"
                                       "PRIMARY KEY (step, variable, operation));";

    return execute(sqlCreateTable);
  }

  void insertOrUpdateDerivedQuantity(int step, const std::string& variable,
                                     const std::string& operation, const std::string& blob_name,
                                     const std::string& bucket_name, float value) {
    sqlite3_stmt* stmt;
    std::string sqlInsertOrUpdate = R"(
        INSERT INTO derived_targets (step, variable, operation, blob_name, bucket_name, value)
        VALUES (?, ?, ?, ?, ?, ?)
        ON CONFLICT(step, variable, operation) DO UPDATE SET
            blob_name = excluded.blob_name,
            bucket_name = excluded.bucket_name,
            value = CASE
                WHEN operation = 'min' AND excluded.value < value THEN excluded.value
                WHEN operation = 'max' AND excluded.value > value THEN excluded.value
                ELSE value
            END;
    )";

    if (sqlite3_prepare_v2(db, sqlInsertOrUpdate.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_int(stmt, 1, step);
      sqlite3_bind_text(stmt, 2, variable.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, operation.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 4, blob_name.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 5, bucket_name.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_double(stmt, 6, static_cast<double>(value));

      if (sqlite3_step(stmt) != SQLITE_DONE) {
        // Handle error: You can use sqlite3_errmsg(db) to get the text of the error message
      }

      sqlite3_finalize(stmt);
    } else {
      // Handle error: You can use sqlite3_errmsg(db) to get the text of the error message
    }
  }

};

#endif //COEUS_INCLUDE_COMMON_SQLITE_H_
