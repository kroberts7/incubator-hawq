#include "sql-util.h"

#include <unistd.h>

#include <fstream>
#include <iostream>
#include <vector>

#include "string-util.h"

SQLUtility::SQLUtility()
    : conn(getConnection()),
      testRootPath(getTestRootPath()),
      test_info(::testing::UnitTest::GetInstance()->current_test_info()) {
  schemaName =
      std::string(test_info->test_case_name()) + "_" + test_info->name();
  conn->runSQLCommand("DROP SCHEMA IF EXISTS " + schemaName + " CASCADE");
  conn->runSQLCommand("CREATE SCHEMA " + schemaName);
}

SQLUtility::~SQLUtility() {
  if (!test_info->result()->Failed())
    conn->runSQLCommand("DROP SCHEMA " + schemaName + " CASCADE");
}

void SQLUtility::execute(const std::string &sql) {
  EXPECT_EQ((conn->runSQLCommand("SET SEARCH_PATH=" + schemaName + ";" + sql))
                .getLastStatus(),
            0)
      << conn->getLastResult();
}

void SQLUtility::query(const std::string &sql, int expectNum) {
  const PSQLQueryResult &result = executeQuery(sql);
  ASSERT_FALSE(result.isError()) << result.getErrorMessage();
  EXPECT_EQ(expectNum, result.rowCount());
}

void SQLUtility::query(const std::string &sql, const std::string &expectStr) {
  const PSQLQueryResult &result = executeQuery(sql);
  ASSERT_FALSE(result.isError()) << result.getErrorMessage();
  std::vector<std::vector<std::string> > resultString = result.getRows();
  std::string resultStr;
  for (auto row : result.getRows()) {
    for (auto column : row) resultStr += column + "|";
    resultStr += "\n";
  }
  EXPECT_EQ(expectStr, resultStr);
}

void SQLUtility::execSQLFile(const std::string &sqlFile,
                             const std::string &ansFile) {
  // do precheck for sqlFile & ansFile
  if (StringUtil::StartWith(sqlFile, "/") ||
      StringUtil::StartWith(ansFile, "/"))
    ASSERT_TRUE(false) << "For sqlFile and ansFile, relative path to feature "
                          "test root dir is needed";
  std::string ansFileAbsPath = testRootPath + "/" + ansFile;
  if (!std::ifstream(ansFileAbsPath))
    ASSERT_TRUE(false) << ansFileAbsPath << " doesn't exist";
  FilePath fp = splitFilePath(ansFileAbsPath);
  // double check to avoid empty fileBaseName
  if (fp.fileBaseName.empty())
    ASSERT_TRUE(false) << ansFileAbsPath << " is invalid";

  // generate new sql file with set search_path added at the begining
  const std::string newSqlFile = generateSQLFile(sqlFile);

  // outFile is located in the same folder with ansFile
  std::string outFile = fp.path + "/" + fp.fileBaseName + ".out";
  conn->setOutputFile(outFile);
  EXPECT_EQ(conn->runSQLFile(newSqlFile).getLastStatus(), 0);
  conn->resetOutput();
  if (remove(newSqlFile.c_str()))
    ASSERT_TRUE(false) << "Error deleting file " << newSqlFile;
  EXPECT_FALSE(conn->checkDiff(ansFile, outFile, true));
}

std::unique_ptr<PSQL> SQLUtility::getConnection() {
  std::unique_ptr<PSQL> psql(
      new PSQL(HAWQ_DB, HAWQ_HOST, HAWQ_PORT, HAWQ_USER, HAWQ_PASSWORD));
  return std::move(psql);
}

const std::string SQLUtility::generateSQLFile(const std::string &sqlFile) {
  const std::string originSqlFile = testRootPath + "/" + sqlFile;
  const std::string newSqlFile = "/tmp/" + schemaName + ".sql";
  std::fstream in;
  in.open(originSqlFile, std::ios::in);
  if (!in.is_open()) {
    EXPECT_TRUE(false) << "Error opening file " << originSqlFile;
  }
  std::fstream out;
  out.open(newSqlFile, std::ios::out);
  if (!out.is_open()) {
    EXPECT_TRUE(false) << "Error opening file " << newSqlFile;
  }
  out << "-- start_ignore" << std::endl
      << "SET SEARCH_PATH=" + schemaName + ";" << std::endl
      << "-- end_ignore" << std::endl;
  std::string line;
  while (getline(in, line)) {
    out << line;
  }
  in.close();
  out.close();
  return newSqlFile;
}

const PSQLQueryResult &SQLUtility::executeQuery(const std::string &sql) {
  const PSQLQueryResult &result =
      conn->getQueryResult("SET SEARCH_PATH=" + schemaName + ";" + sql);
  return result;
}

PSQL *SQLUtility::getPSQL() const { return conn.get(); }

std::string SQLUtility::getTestRootPath() const {
  FilePath fp = splitFilePath(__FILE__);
  return splitFilePath(__FILE__).path + "/..";
}

FilePath SQLUtility::splitFilePath(const std::string &filePath) const {
  FilePath fp;
  size_t found1 = filePath.find_last_of("/");
  size_t found2 = filePath.find_last_of(".");
  fp.path = filePath.substr(0, found1);
  fp.fileBaseName = filePath.substr(found1 + 1, found2 - found1 - 1);
  fp.fileSuffix = filePath.substr(found2 + 1, filePath.length() - found2 - 1);
  return fp;
}
