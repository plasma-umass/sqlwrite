/*

  SQLwrite
 
  by Emery Berger <https://emeryberger.com>

  Translates natural-language queries to SQL.
  
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include <fmt/core.h>
#include <fmt/format.h>

#include "openai.hpp"
#include "sqlite3ext.h"
#include "aistream.hpp"

SQLITE_EXTENSION_INIT1;

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

// Function to rephrase a query using ChatGPT
std::list<std::string> rephraseQuery(ai::ai_stream& ai, const std::string& query, int n = 10)
{
  // Query the ChatGPT model for rephrasing
  auto promptq = fmt::format("Rephrase the following query {} times, all using different wording. Produce a JSON object with the result as a list with the field \"Rewording\". Query to rephrase: '{}'", n, query);

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

static void real_ask_command(sqlite3_context *ctx, int argc, sqlite3_value **argv) {

  /* ---- build a query prompt to translate from natural language to SQL. ---- */
  // The prompt consists of all table names and schemas, plus any indexes, along with directions.
  
  // Print all loaded database schemas
  sqlite3 *db = sqlite3_context_db_handle(ctx);
  sqlite3_stmt *stmt;

  auto nl_to_sql = fmt::format("Given a database with the following tables, schemas, and indexes, write a SQL query in SQLite's SQL dialect that answers this question or produces the desired report: '{}'. Produce a JSON object with the SQL query as a field \"SQL\". Offer a list of suggestions as SQL commands to create indexes that would improve query performance in a field \"Indexing\". Do so only if those indexes are not already given in 'Existing indexes'. Only produce output that can be parsed as JSON.\n\nSchemas:\n", (const char *) sqlite3_value_text(argv[0]));
  
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
    return;
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

  ai::ai_stream ai ({ .maxRetries = 3 }); // , .debug = true });

  ai << ai::ai_config::GPT_35;
  
  /* ----  translate the natural language query to SQL and execute it (and request indexes) ---- */
  
  ai << json({
      { "role", "assistant" },
	{ "content", "You are a programming assistant who is an expert in generating SQL queries from natural language. You ONLY respond with JSON objects." }
    });

  ai << json({
      { "role", "user" },
	{ "content", nl_to_sql.c_str() }
    });
 
  std::string sql_translation;
  
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
  ai << ai::ai_validator([](const json& json_result){
    try {
      auto translation = json_result["Translation"].get<std::string>();
      return true;
    } catch (std::exception& e) {
      return false;
    }
  });
  ai >> json_result;
  auto translation = json_result["Translation"].get<std::string>();
  std::cout << fmt::format("[SQLwrite] translation of SQL query back to natural language: {}\n", translation.c_str());

  /* ---- get N translations from natural language to compare results ---- */
  auto rewordings = rephraseQuery(ai, sql_translation, 10);
  for (auto& w : rewordings) {
    std::cout << "Rewording: " << w << std::endl;
  }
  
  // TODO
  
  sqlite3_finalize(stmt);
}
     
static void ask_command(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
  if (argc != 1) {
    sqlite3_result_error(ctx, "The 'ask' command takes exactly one argument.", -1);
  }
  real_ask_command(ctx, argc, argv);
}


static void sqlwrite_command(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
  if (argc != 1) {
    sqlite3_result_error(ctx, "The 'sqlwrite' command takes exactly one argument.", -1);
  }
  real_ask_command(ctx, argc, argv);
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
