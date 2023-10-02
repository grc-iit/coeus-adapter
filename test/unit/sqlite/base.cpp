/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This blob is part of Coeus-adapter. The full Coeus-adapter copyright      *
 * notice, including terms governing use, modification, and redistribution,  *
 * is contained in the COPYING blob, which can be found at the top directory.*
 * If you do not have access to the blob, you may request a copy             *
 * from scslab@iit.edu.                                                      *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <gtest/gtest.h>
#include <common/SQlite.h>

class SQLiteWrapperTest : public ::testing::Test {
 protected:
  SQLiteWrapper db;

  SQLiteWrapperTest() : db("test_metadata.db", true) {}

  virtual void SetUp() {
    db.createAppsTable();
    db.createBlobLocationsTable();
    db.createVariableMetadataTable();
  }

  virtual void TearDown() {
    // Clean up any necessary resources
    // For this example, we'll just leave the database as-is
  }
};

TEST_F(SQLiteWrapperTest, Open) {
  SQLiteWrapper db("Open.db", true);
  EXPECT_TRUE(db.isOpen());
}

TEST_F(SQLiteWrapperTest, TestUpdateAndGetTotalSteps) {
  db.UpdateTotalSteps("TestApp", 5);
  ASSERT_EQ(db.GetTotalSteps("TestApp"), 5);
}

TEST_F(SQLiteWrapperTest, TestInsertAndGetBlobLocation) {
  BlobInfo blobInfo("TestBucket", "TestBlob");
  db.InsertBlobLocation(1, 0, "TestVar", blobInfo);
  BlobInfo retrievedBlobInfo = db.GetBlobLocation(1, 0, "TestVar");
  ASSERT_EQ(retrievedBlobInfo.bucket_name, "TestBucket");
  ASSERT_EQ(retrievedBlobInfo.blob_name, "TestBlob");
}

TEST_F(SQLiteWrapperTest, TestInsertAndGetVariableMetadata) {
  VariableMetadata metadata("TestVar", {4, 4}, {0, 0}, {4, 4}, true, "int");
  db.InsertVariableMetadata(1, 0, metadata);
  VariableMetadata retrievedMetadata = db.GetVariableMetadata(1, 0, "TestVar");
  ASSERT_EQ(retrievedMetadata.name, "TestVar");
  ASSERT_EQ(retrievedMetadata.dataType, "int");
  ASSERT_EQ(retrievedMetadata.shape[0], 4);
  ASSERT_EQ(retrievedMetadata.start[0], 0);
  ASSERT_EQ(retrievedMetadata.count[0], 4);
  ASSERT_TRUE(retrievedMetadata.constantShape);
}

TEST_F(SQLiteWrapperTest, TestNonExistentApp) {
  ASSERT_EQ(db.GetTotalSteps("NonExistentApp"), -1);
}

TEST_F(SQLiteWrapperTest, TestNonExistentBlobLocation) {
  BlobInfo retrievedBlobInfo = db.GetBlobLocation(99, 99, "NonExistentVar");
  ASSERT_TRUE(retrievedBlobInfo.bucket_name.empty());
  ASSERT_TRUE(retrievedBlobInfo.blob_name.empty());
}

TEST_F(SQLiteWrapperTest, TestNonExistentVariableMetadata) {
  VariableMetadata retrievedMetadata = db.GetVariableMetadata(99, 99, "NonExistentVar");
  ASSERT_TRUE(retrievedMetadata.name.empty());
}

TEST_F(SQLiteWrapperTest, TestUpdateTotalStepsWithEmptyAppName) {
  db.UpdateTotalSteps("", 5);
  ASSERT_EQ(db.GetTotalSteps(""), -1);  // Expecting not to find an app with an empty name
}

TEST_F(SQLiteWrapperTest, TestInsertBlobLocationWithEmptyVarName) {
  BlobInfo blobInfo("TestBucket", "TestBlob");
  db.InsertBlobLocation(1, 0, "", blobInfo);
  BlobInfo retrievedBlobInfo = db.GetBlobLocation(1, 0, "");
  ASSERT_TRUE(retrievedBlobInfo.bucket_name.empty());
  ASSERT_TRUE(retrievedBlobInfo.blob_name.empty());
}

TEST_F(SQLiteWrapperTest, TestInsertVariableMetadataWithEmptyVarName) {
  VariableMetadata metadata("", {4, 4}, {0, 0}, {4, 4}, true, "int");
  db.InsertVariableMetadata(1, 0, metadata);
  VariableMetadata retrievedMetadata = db.GetVariableMetadata(1, 0, "");
  ASSERT_TRUE(retrievedMetadata.name.empty());
}

TEST_F(SQLiteWrapperTest, TestInsertAndGetVariableMetadataWithLargeData) {
  std::vector<size_t> largeVector(100000, 1);  // A large vector for testing
  VariableMetadata metadata("LargeVar", largeVector, largeVector, largeVector, true, "int");
  db.InsertVariableMetadata(1, 0, metadata);
  VariableMetadata retrievedMetadata = db.GetVariableMetadata(1, 0, "LargeVar");
  ASSERT_EQ(retrievedMetadata.name, "LargeVar");
  ASSERT_EQ(retrievedMetadata.shape.size(), 100000);
}

TEST_F(SQLiteWrapperTest, TestInsertBlobLocationWithSpecialCharacters) {
  BlobInfo blobInfo("TestBucket", "BlobWithSpecialChars!@#$%^&*()");
  db.InsertBlobLocation(1, 0, "TestVar", blobInfo);
  BlobInfo retrievedBlobInfo = db.GetBlobLocation(1, 0, "TestVar");
  ASSERT_EQ(retrievedBlobInfo.bucket_name, "TestBucket");
  ASSERT_EQ(retrievedBlobInfo.blob_name, "BlobWithSpecialChars!@#$%^&*()");
}

TEST_F(SQLiteWrapperTest, TestInsertVariableMetadataWithSpecialCharacters) {
  VariableMetadata metadata("VarWithSpecialChars!@#$%^&*()", {4, 4}, {0, 0}, {4, 4}, true, "int");
  db.InsertVariableMetadata(1, 0, metadata);
  VariableMetadata retrievedMetadata = db.GetVariableMetadata(1, 0, "VarWithSpecialChars!@#$%^&*()");
  ASSERT_EQ(retrievedMetadata.name, "VarWithSpecialChars!@#$%^&*()");
}

//3 different derived quantities:
//1) absolute derived: module variable (application) Adios
//2) relative derived: Difference module between two steps (application + stored data) Coeus
//3) storage derived:  (stored data) Hermes