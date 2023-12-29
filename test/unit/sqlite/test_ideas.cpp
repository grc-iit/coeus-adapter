//
// Created by jaime on 9/29/2023.
//

TEST(SQLiteTest, Distribution) {
SQLiteWrapper<double> db("Distribution.db", true);
EXPECT_TRUE(db.isOpen());
db.createMetadataTable();
EXPECT_TRUE(db.insertMetadata(0, 4, "blob1", 0));
EXPECT_TRUE(db.insertMetadata(4, 4, "blob2", 0));
}

TEST(SQLiteTest, DistributionQueryEven) {
SQLiteWrapper<double> db("DistributionQueryEven.db", true);
EXPECT_TRUE(db.isOpen());
db.createMetadataTable();
EXPECT_TRUE(db.insertMetadata(0, 4, "blob1", 0));
EXPECT_TRUE(db.insertMetadata(4, 4, "blob2", 0));
auto blobs = db.queryblobs(0, 4);
ASSERT_EQ(blobs.size(), 1);
ASSERT_EQ(blobs[0], "blob1");
}

TEST(SQLiteTest, DistributionQueryUneven) {
SQLiteWrapper<double> db("DistributionQueryUneven.db", true);
EXPECT_TRUE(db.isOpen());
db.createMetadataTable();
EXPECT_TRUE(db.insertMetadata(0, 4, "blob1", 0));
EXPECT_TRUE(db.insertMetadata(4, 4, "blob2", 0));
auto blobs = db.queryblobs(0, 5);
ASSERT_EQ(blobs.size(), 2);
ASSERT_EQ(blobs[0], "blob1");
ASSERT_EQ(blobs[1], "blob2");
}

TEST(SQLiteTest, DistributionQueryLarge) {
SQLiteWrapper<double> db("DistributionQueryLarge.db", true);
EXPECT_TRUE(db.isOpen());
db.createMetadataTable();
//InsertMetadata(startingIndex, size, blobName)
EXPECT_TRUE(db.insertMetadata(0, 4, "blob1", 0));
EXPECT_TRUE(db.insertMetadata(4, 4, "blob2", 0));
EXPECT_TRUE(db.insertMetadata(8, 4, "blob3", 0));
EXPECT_TRUE(db.insertMetadata(12, 4, "blob4", 0));
EXPECT_TRUE(db.insertMetadata(16, 4, "blob5", 0));
EXPECT_TRUE(db.insertMetadata(20, 4, "blob6", 0));
EXPECT_TRUE(db.insertMetadata(24, 4, "blob7", 0));

auto blobs = db.queryblobs(12, 16);
ASSERT_EQ(blobs.size(), 1);
ASSERT_EQ(blobs[0], "blob4");

blobs = db.queryblobs(13, 17);
ASSERT_EQ(blobs.size(), 2);
ASSERT_EQ(blobs[0], "blob4");
ASSERT_EQ(blobs[1], "blob5");

blobs = db.queryblobs(0, 28);
ASSERT_EQ(blobs.size(), 7);

blobs = db.queryblobs(0, 14);
ASSERT_EQ(blobs.size(), 4);

blobs = db.queryblobs(15, 21);
ASSERT_EQ(blobs.size(), 3);
}

TEST(SQLiteTest, DistributionQueryLimits) {
SQLiteWrapper<double> db("DistributionQueryLimits.db", true);
EXPECT_TRUE(db.isOpen());
db.createMetadataTable();
EXPECT_TRUE(db.insertMetadata(0, 4, "blob1", 0));
EXPECT_TRUE(db.insertMetadata(4, 4, "blob2", 0));

auto blobs = db.queryblobs(-3, -1);
ASSERT_EQ(blobs.size(), 0);

blobs = db.queryblobs(8, 12);
ASSERT_EQ(blobs.size(), 0);

blobs = db.queryblobs(-3, 2);
ASSERT_EQ(blobs.size(), 1);
ASSERT_EQ(blobs[0], "blob1");

blobs = db.queryblobs(5, 10);
ASSERT_EQ(blobs.size(), 1);
ASSERT_EQ(blobs[0], "blob2");
}

TEST(SQLiteTest, MinMaxTestInt) {
SQLiteWrapper<int> db("test_minmax_int.db", true);

ASSERT_TRUE(db.createSegmentDataTable());
ASSERT_TRUE(db.insertSegmentData(1, 5, "blob1"));
ASSERT_TRUE(db.insertSegmentData(3, 8, "blob2"));
ASSERT_TRUE(db.insertSegmentData(0, 6, "blob3"));

auto [globalMin, globalMax] = db.queryGlobalMinMax();
ASSERT_EQ(globalMin, 0);
ASSERT_EQ(globalMax, 8);
}

TEST(SQLiteTest, MinMaxTestDouble) {
SQLiteWrapper<double> db("test_minmax_double.db", true);

ASSERT_TRUE(db.createSegmentDataTable());
ASSERT_TRUE(db.insertSegmentData(1.2, 5.8, "blob1"));
ASSERT_TRUE(db.insertSegmentData(3.5, 8.6, "blob2"));
ASSERT_TRUE(db.insertSegmentData(0.9, 6.3, "blob3"));

auto [globalMin, globalMax] = db.queryGlobalMinMax();
ASSERT_DOUBLE_EQ(globalMin, 0.9);
ASSERT_DOUBLE_EQ(globalMax, 8.6);
}

TEST(SQLiteTest, QueryFilesTest) {
SQLiteWrapper<int> db("test_query_files.db", true);

ASSERT_TRUE(db.createMetadataTable());
ASSERT_TRUE(db.insertMetadata(0, 4, "file1", 0));  // Elements: 0, 1, 2, 3
ASSERT_TRUE(db.insertMetadata(4, 4, "file2", 0));  // Elements: 4, 5, 6, 7
ASSERT_TRUE(db.insertMetadata(8, 4, "file3", 0));  // Elements: 8, 9, 10, 11

auto result1 = db.queryBlobsWithOffset(2, 10);
ASSERT_EQ(result1.size(), 3);
ASSERT_EQ(result1[0].first, "file1");
ASSERT_EQ(result1[0].second.first, 2);
ASSERT_EQ(result1[0].second.second, 4);
ASSERT_EQ(result1[1].first, "file2");
ASSERT_EQ(result1[1].second.first, 0);
ASSERT_EQ(result1[1].second.second, 4);
ASSERT_EQ(result1[2].first, "file3");
ASSERT_EQ(result1[2].second.first, 0);
ASSERT_EQ(result1[2].second.second, 2);

auto result2 = db.queryBlobsWithOffset(5, 9);
ASSERT_EQ(result2.size(), 2);
ASSERT_EQ(result2[0].first, "file2");
ASSERT_EQ(result2[0].second.first, 1);
ASSERT_EQ(result2[0].second.second, 4);
ASSERT_EQ(result2[1].first, "file3");
ASSERT_EQ(result2[1].second.first, 0);
ASSERT_EQ(result2[1].second.second, 1);
}

TEST(SQLiteTest, QueryBlobsByStepTest) {
SQLiteWrapper<int> db("test_query_blobs.db", true);

ASSERT_TRUE(db.createMetadataTable());
ASSERT_TRUE(db.insertMetadata(0, 4, "blob1", 1));
ASSERT_TRUE(db.insertMetadata(4, 4, "blob2", 2));
ASSERT_TRUE(db.insertMetadata(8, 4, "blob3", 1));

auto blobsForStep1 = db.queryBlobsByStep(1);
ASSERT_EQ(blobsForStep1.size(), 2);
ASSERT_EQ(blobsForStep1[0], "blob1");
ASSERT_EQ(blobsForStep1[1], "blob3");

auto blobsForStep2 = db.queryBlobsByStep(2);
ASSERT_EQ(blobsForStep2.size(), 1);
ASSERT_EQ(blobsForStep2[0], "blob2");
}

TEST(SQLiteWrapperTest, GlobalTableTest) {
SQLiteWrapper<int> db("test_global_table.db", true);

ASSERT_TRUE(db.createGlobalTable());
ASSERT_TRUE(db.insertGlobalData("App1", "VarA", false, 4, true, "int", {4, 4}));
ASSERT_TRUE(db.insertGlobalData("App1", "VarB", false, 2, false, "double", {8, 2}));
ASSERT_TRUE(db.insertGlobalData("App2", "VarA", false, 3, true, "float", {3, 3, 3}));

ASSERT_EQ(db.queryNumberOfApps(), 2);

auto app1Variables = db.queryVariablesByApp("App1");
ASSERT_EQ(app1Variables.size(), 2);
ASSERT_EQ(app1Variables[0], "VarA");
ASSERT_EQ(app1Variables[1], "VarB");

auto varAInfo = db.queryInfoByVariableName("VarA");
ASSERT_EQ(varAInfo.app_name, "App1");
ASSERT_TRUE(varAInfo.derived);
ASSERT_EQ(varAInfo.num_processes, 4);
ASSERT_TRUE(varAInfo.constant_shape);
ASSERT_EQ(varAInfo.data_type, "int");
ASSERT_EQ(varAInfo.dimensions, std::vector<size_t>({4, 4}));
}