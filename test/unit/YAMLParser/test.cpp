//
// Created by jaime on 9/7/2023.
//

#include <gtest/gtest.h>
#include "common/YAMLParser.h"

// Test case for parsing a correct YAML file
TEST(YAMLParserTest, CorrectYAML) {
  YAMLParser parser("samples/correct.yaml");
  ASSERT_TRUE(parser.isValid());

  YAMLMap result = parser.parse();
  ASSERT_EQ(result.size(), 2);

  EXPECT_EQ(result["variable1"]["operator1"], "function1");
  EXPECT_EQ(result["variable1"]["operator2"], "function2");
  EXPECT_EQ(result["variable2"]["operator3"], "function3");
  EXPECT_EQ(result["variable2"]["operator4"], "function4");
}

// Test case for parsing an incorrect YAML file
TEST(YAMLParserTest, IncorrectYAML) {
  try {
    YAMLParser parser("samples/incorrect.yaml");
    FAIL() << "Expected an exception due to invalid YAML";
  } catch (const ErrorException& e) {
    std::string error_message(e.what());
    EXPECT_EQ(error_message, BAD_YAML.getMessage());
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
