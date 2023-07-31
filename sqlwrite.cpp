/*

  SQLwrite
 
  by Emery Berger <https://emeryberger.com>

  Translates natural-language queries to SQL.
  
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

#include <string>
#include <vector>

#include <openssl/sha.h>
#include <openssl/evp.h>

#include <fmt/core.h>
#include <fmt/format.h>

#include "openai.hpp"
#include "sqlite3ext.h"
#include "aistream.hpp"

SQLITE_EXTENSION_INIT1;

// Function to calculate SHA256 hash of a string
std::string calculateSHA256Hash(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();

    EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(mdctx, input.c_str(), input.size());
    EVP_DigestFinal_ex(mdctx, hash, nullptr);

    EVP_MD_CTX_free(mdctx);

    std::string hashString;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hashString += fmt::format("{:02x}", hash[i]);
    }

    return hashString;
}

// SQLite callback function to retrieve query results
int callback(void* data, int argc, char** argv, char** /* azColName */) {
    std::list<std::string>& resultList = *static_cast<std::list<std::string>*>(data);

    for (int i = 0; i < argc; i++) {
        if (argv[i]) {
            resultList.push_back(argv[i]);
        }
    }

    return 0;
}

// Function to execute a query using SQLite
std::list<std::string> executeQuery(const std::string& query) {
    std::list<std::string> resultList;
    sqlite3* db;
    char* errMsg = nullptr;

    // Open the database connection
    int rc = sqlite3_open(":memory:", &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Error opening database: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return resultList;
    }

    // Execute the query and retrieve the results
    rc = sqlite3_exec(db, query.c_str(), callback, &resultList, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Error executing query: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }

    // Close the database connection
    sqlite3_close(db);

    return resultList;
}

int print_em(void* data, int c_num, char** c_vals, char** c_names) {
    for (int i = 0; i < c_num; i++) {
      std::cout << (c_vals[i] ? c_vals[i] : "NULL");
      if ((i < c_num-1) && (c_num > 1)) {
	std::cout << "|";
      }
    }
    std::cout << std::endl;
    return 0;
}

#include <iostream>
#include <string>

std::string removeEscapedCharacters(const std::string& s) {
    std::string result = s;
    std::size_t pos = result.find("\\");
    while (pos != std::string::npos) {
        if (result[pos+1] == 'n') { // Check if escaped newline
            result.replace(pos, 2, " "); // Replace with single space
        } else if (result[pos+1] == '"') { // Check if escaped quote
            result.erase(pos, 1); // Remove backslash
        }
        pos = result.find("\\", pos+1); // Find next escaped character
    }
    return result;
}


std::string removeEscapedNewlines(const std::string& s) {
    std::string result = s;
    std::size_t pos = result.find("\\n");
    while (pos != std::string::npos) {
      result.replace(pos, 2, " ");
        pos = result.find("\\n", pos);
    }
    return result;
}


nlohmann::json sampleSQLiteDistinct(sqlite3* DB, int N) {
    nlohmann::json result;

    // Query for all tables in the database
    std::string tables_query = "SELECT name FROM sqlite_master WHERE type='table';";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(DB, tables_query.c_str(), -1, &stmt, 0);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string table_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));

        // Query for column names in current table
        std::string columns_query = fmt::format("PRAGMA table_info({});", table_name);
        sqlite3_stmt* column_stmt;
        sqlite3_prepare_v2(DB, columns_query.c_str(), -1, &column_stmt, 0);

        while (sqlite3_step(column_stmt) == SQLITE_ROW) {
            std::string column_name = reinterpret_cast<const char*>(sqlite3_column_text(column_stmt, 1));

            // Query for N random distinct values from current column
            std::string values_query = fmt::format("SELECT DISTINCT {} FROM {} ORDER BY RANDOM() LIMIT {};", column_name, table_name, N);
            sqlite3_stmt* values_stmt;
            sqlite3_prepare_v2(DB, values_query.c_str(), -1, &values_stmt, 0);

	    std::vector<std::string> column_values;
	    while (sqlite3_step(values_stmt) == SQLITE_ROW) {
	      const char* data = reinterpret_cast<const char*>(sqlite3_column_text(values_stmt, 0));
	      if (data != nullptr) {
		column_values.push_back(data);
	      } else {
		column_values.push_back("NULL");  // Or however you want to represent null values
	      }
	    }

            result[table_name][column_name] = column_values;
            sqlite3_finalize(values_stmt);
        }
        sqlite3_finalize(column_stmt);
    }
    sqlite3_finalize(stmt);

    return result;
}


// Function to rephrase a query using ChatGPT
std::list<std::string> rephraseQuery(ai::ai_stream& ai, const std::string& query, int n = 10)
{
  // Query the ChatGPT model for rephrasing
  auto promptq = fmt::format("Rephrase the following query {} times, all using different wording. Produce a JSON object with the result as a list with the field \"Rewording\". Do not include any SQL in any rewording. Query to rephrase: '{}'", n, query);

  ai.reset();
  ai << json({
      { "role", "assistant" },
	{ "content", "You are an assistant who is an expert in rewording natural language expressions. You ONLY respond with JSON objects." }
    });
  ai << json({
      {"role", "user" },
	{"content", promptq.c_str() }
    });
  ai << ai::ai_validator([](const json& j) {
    // Enforce list output
    volatile auto list = j["Rewording"].get<std::list<std::string>>();
    return true;
  });

  json json_response;
  ai >> json_response;
  
  // Parse the response and extract the rephrased queries
  auto rephrasedQueries = json_response["Rewording"].get<std::list<std::string>>();
  return rephrasedQueries;
}

static bool translateQuery(ai::ai_stream& ai,
			   sqlite3_context *ctx,
			   int argc,
			   const char * query,
			   json& json_response,
			   std::string& sql_translation)
{
  
  /* ---- build a query prompt to translate from natural language to SQL. ---- */
  // The prompt consists of all table names and schemas, plus any indexes, along with directions.
  
  // Print all loaded database schemas
  sqlite3 *db = sqlite3_context_db_handle(ctx);
  sqlite3_stmt *stmt;

  // auto nl_to_sql = fmt::format("Given a database with the following tables, schemas, and indexes, write a SQL query in SQLite's SQL dialect that answers this question or produces the desired report: '{}'. Produce a JSON object with the SQL query as a field \"SQL\". Offer a list of suggestions as SQL commands to create indexes that would improve query performance in a field \"Indexing\". Do so only if those indexes are not already given in 'Existing indexes'. Only produce output that can be parsed as JSON.\n\nSchemas:\n", query);
  
  auto nl_to_sql = fmt::format("Given a database with the following tables, schemas, indexes, and samples for each column, write a SQL query in SQLite's SQL dialect that answers this question or produces the desired report: '{}'. Produce a JSON object with the SQL query as a field \"SQL\". Offer a list of suggestions as SQL commands to create indexes that would improve query performance in a field \"Indexing\". Do so only if those indexes are not already given in 'Existing indexes'. Only produce output that can be parsed as JSON.\n\nSchemas:\n", query);
  
  sqlite3_prepare_v2(db, "SELECT name, sql FROM sqlite_master WHERE type='table' OR type='view'", -1, &stmt, NULL);

  auto total_tables = 0;
  
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    const char *sql = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    nl_to_sql += fmt::format("Schema for {}: {}\n", name, sql);
    total_tables++;
  }

  // Fail gracefully if no databases are present.
  if (total_tables == 0) {
    std::cout << "SQLwrite: you need to load a table first." << std::endl;
    sqlite3_finalize(stmt);
    return false;
  }

  // Add indexes, if any.

  auto printed_index_header = false;
  sqlite3_prepare_v2(db, "SELECT type, name, tbl_name, sql FROM sqlite_master WHERE type='index'", -1, &stmt, NULL);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (!printed_index_header) {
      nl_to_sql += "\n\nExisting indexes:\n";
      printed_index_header = true;
    }
    try {
      const char *tbl_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
      const char *sql = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
      nl_to_sql += fmt::format("Index for {}: {}\n", tbl_name, sql);
    } catch (fmt::v9::format_error& fe) {
      // Ignore indices where the query response is null, which could get us here.
    }
  }

  // Randomly sample values from the database.
  auto sample_value_json = sampleSQLiteDistinct(db, 5); // magic number FIXME
  nl_to_sql += fmt::format("\nSample values for each column: {}\n", sample_value_json.dump());
  
  /* ----  translate the natural language query to SQL and execute it (and request indexes) ---- */
  
  ai << json({
      { "role", "assistant" },
	{ "content", "You are a programming assistant who is an expert in generating SQL queries from natural language. You ONLY respond with JSON objects." }
    });

  ai << json({
      { "role", "user" },
	{ "content", nl_to_sql.c_str() }
    });
  
  ai << ai::ai_validator([&](const json& j) {
    try {
      // Ensure we got a SQL response.
      sql_translation = j["SQL"].get<std::string>();
      // Iterate through indexes to ensure validity.
      for (auto& item : j["Indexing"]) {
	volatile auto item_test = item.get<std::string>();
      }
    } catch (std::exception& e) {
      return false;
    }
    // Remove any escaped newlines.
    sql_translation = removeEscapedNewlines(sql_translation);
    sql_translation = removeEscapedCharacters(sql_translation);
    
    // Send the query to the database.
    auto rc = sqlite3_exec(db, sql_translation.c_str(), [](void*, int, char**, char**) {return 0;}, nullptr, nullptr);
    if (rc != SQLITE_OK) {
      std::cerr << fmt::format("[SQLwrite] Error executing SQL statement \"{}\":\n           {}\n", sql_translation.c_str(), sqlite3_errmsg(db));
      // sqlite3_finalize(stmt);
    }
    return rc == SQLITE_OK;
  });

  json json_result;
  ai >> json_result;

  return true;
}

static void real_ask_command(sqlite3_context *ctx, int argc, const char * query) { //  sqlite3_value **argv) {

  sqlite3 *db = sqlite3_context_db_handle(ctx);
  json json_result;
  std::string sql_translation;
  
  // ai::ai_stream ai ({ .maxRetries = 3 , .debug = true });
  ai::ai_stream ai ({ .maxRetries = 3 });

  ai << ai::ai_config::GPT_3_5;
  
  bool r = translateQuery(ai, ctx, argc, query, json_result, sql_translation);
  if (!r) {
    std::cerr << "[SQLwrite] translation error." << std::endl;
    return;
  }
  
  // Send the query to the database, printing it this time.
  auto rc = sqlite3_exec(db, sql_translation.c_str(), print_em, nullptr, nullptr);
  
  std::cout << fmt::format("[SQLwrite] translation to SQL: {}\n", sql_translation.c_str());
  
  if (json_result["Indexing"].size() > 0) {
    std::cout << "[SQLwrite] indexing suggestions to improve the performance for this query:" << std::endl;
    int i = 0;
    for (auto& item : json_result["Indexing"]) {
      i++;
      std::cout << fmt::format("({}): {}\n", i, item.get<std::string>());
    }
  }

  /* ----  translate the SQL query back to natural language ---- */
  
  ai.reset();
  ai << json({
      { "role", "assistant" },
	{ "content", "You are a programming assistant who is an expert in translating SQL queries to natural language. You ONLY respond with JSON objects." }
    });

  auto translate_to_natural_language_query = fmt::format("Given the following SQL query, convert it into natural language: '{}'. Produce a JSON object with the translation as a field \"Translation\". Only produce output that can be parsed as JSON.\n", sql_translation);
  ai << json({
      { "role", "user" },
      { "content", translate_to_natural_language_query.c_str() }
    });
  std::string translation;
  ai << ai::ai_validator([&](const json& json_result){
    try {
      translation = json_result["Translation"].get<std::string>();
      return true;
    } catch (std::exception& e) {
      return false;
    }
  });
  ai >> json_result;
  //  auto translation = json_result["Translation"].get<std::string>();
  std::cout << fmt::format("[SQLwrite] translation back to natural language: {}\n", translation.c_str());

#if 0 // disable temporarily
  /* ---- get N translations from natural language to compare results ---- */
  const auto N = 10;
  auto rewordings = rephraseQuery(ai, sql_translation, N);
  auto values = new const char *[N];
  int i = 0;
  for (const auto& w : rewordings) {
    values[i] = w.c_str();
    std::cerr << "Rewording " << i << " = " << (const char *) values[i] << std::endl;
    i++;
  }
  std::cerr << "Trying " << values[0] << std::endl;
  auto translated = translateQuery(ai, ctx, 1, values[0], json_result, sql_translation);
  std::cout << "Done: translated = " << translated << std::endl;
  // Cleanup allocated values
  delete [] values;
#endif
  //  sqlite3_finalize(stmt);
}
     
static void ask_command(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
  if (argc != 1) {
    sqlite3_result_error(ctx, "The 'ask' command takes exactly one argument.", -1);
  }
  auto query = (const char *) sqlite3_value_text(argv[0]);
  real_ask_command(ctx, argc, query); // argv);
}


static void sqlwrite_command(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
  if (argc != 1) {
    sqlite3_result_error(ctx, "The 'sqlwrite' command takes exactly one argument.", -1);
  }
  auto query = (const char *) sqlite3_value_text(argv[0]);
  real_ask_command(ctx, argc, query);
}


extern "C" int sqlite3_sqlwrite_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi)
{
  openai::start();
  SQLITE_EXTENSION_INIT2(pApi);
    
  int rc;

  rc = sqlite3_create_function(db, "ask", -1, SQLITE_UTF8, db, &ask_command, NULL, NULL);
  if (rc != SQLITE_OK) {
    *pzErrMsg = sqlite3_mprintf("Failed to create ask function: %s", sqlite3_errmsg(db));
    return rc;
  }
  rc = sqlite3_create_function(db, "sqlwrite", -1, SQLITE_UTF8, db, &sqlwrite_command, NULL, NULL);
  if (rc != SQLITE_OK) {
    *pzErrMsg = sqlite3_mprintf("Failed to create sqlwrite function: %s", sqlite3_errmsg(db));
    return rc;
  }
  const char* key = std::getenv("OPENAI_API_KEY");
  if (!key) {
    printf("To use SQLwrite, you must have an API key saved as the environment variable OPENAI_API_KEY.\n");
    printf("For example, run `export OPENAI_API_KEY=sk-...`.\n");
    printf("If you do not have a key, you can get one here: https://openai.com/api/.\n");
    *pzErrMsg = sqlite3_mprintf("OPENAI_API_KEY environment variable not set.\n");
    return SQLITE_ERROR;
  }
  
  printf("SQLwrite extension successfully initialized.\nYou can now use natural language queries like \"select ask('show me all artists.');\".\nPlease report any issues to https://github.com/plasma-umass/sqlwrite/issues/new\n");

  return SQLITE_OK;
}
