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

static void real_ask_command(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
  // Print all loaded database schemas
  sqlite3 *db = sqlite3_context_db_handle(ctx);
  sqlite3_stmt *stmt;

  auto query = fmt::format("Given a database with the following tables, schemas, and indexes, write a SQL query in SQLite's SQL dialect that answers this question or produces the desired report: '{}'. Produce a JSON object with the SQL query as a field \"SQL\". Offer a list of suggestions as SQL commands to create indexes that would improve query performance in a field \"Indexing\". Do so only if those indexes are not already given in 'Existing indexes'. Only produce output that can be parsed as JSON.\n\nSchemas:\n", (const char *) sqlite3_value_text(argv[0]));
  
  sqlite3_prepare_v2(db, "SELECT name, sql FROM sqlite_master WHERE type='table' OR type='view'", -1, &stmt, NULL);

  auto total_tables = 0;
  
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    const char *sql = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    query += fmt::format("Schema for {}: {}\n", name, sql);
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
      query += "\n\nExisting indexes:\n";
      printed_index_header = true;
    }
    try {
      const char *tbl_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
      const char *sql = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
      query += fmt::format("Index for {}: {}\n", tbl_name, sql);
    } catch (fmt::v9::format_error& fe) {
      // Ignore indices where the query response is null, which could get us here.
    }
  }

  ai::ai_stream ai ({ .maxRetries = 3 });

  ai << ai::ai_config::GPT_35;
  
  ai << json({
      { "role", "assistant" },
	{ "content", "You are a programming assistant who is an expert in generating SQL queries from natural language. You ONLY respond with JSON objects." }
    });

  ai << json({
      { "role", "user" },
	{ "content", query.c_str() }
    });
 
  std::string sql_translation;
  
  int attemptsRemaining = 3;
  while (attemptsRemaining) {
    try {
      json json_result;
      ai >> json_result;
      
      //std::cout << "SQL:" << std::endl;
      //std::cout << json_result["SQL"].get<std::string>() << std::endl;
      //std::cout << "Indexing:" << std::endl;
      //std::cout << json_result["Indexing"].dump() << std::endl;
      
      // TODO: subtract set of indexing results from already-indexed columns, and present to user as suggestions
      
      sql_translation = json_result["SQL"].get<std::string>();
      // Remove any escaped newlines.
      sql_translation = removeEscapedNewlines(sql_translation);
      sql_translation = removeEscapedCharacters(sql_translation);
      
      // Send the query to the database.
      auto rc = sqlite3_exec(db, sql_translation.c_str(), print_em, nullptr, nullptr);
      
      if (rc != SQLITE_OK) {
	std::cerr << fmt::format("[SQLwrite] Error executing SQL statement: {}\n", sqlite3_errmsg(db));
	if (attemptsRemaining == 0) {
	  sqlite3_finalize(stmt);
	  return;
	}
	std::cerr << "[SQLwrite] Retrying..." << std::endl;
	attemptsRemaining--;
	continue;
      }
      std::cout << fmt::format("[SQLwrite] translation to SQL: {}\n", sql_translation.c_str());
      
      if (json_result["Indexing"].size() > 0) {
	std::cout << "[SQLwrite] indexing suggestions to improve the performance for this query:" << std::endl;
	int i = 0;
	for (auto& item : json_result["Indexing"]) {
	  i++;
	  std::cout << fmt::format("({}): {}\n", i, item.get<std::string>());
	}
      }
      break;
    } catch (nlohmann::json_abi_v3_11_2::detail::parse_error& pe) {
      // Retry if there were JSON parse errors.
    } catch (nlohmann::json_abi_v3_11_2::detail::type_error& te) {
      // Retry if there were JSON parse errors.
    } catch (fmt::v9::format_error& fe) {
      std::cerr << "error: " << fe.what() << std::endl;
      // Retry if there is a format error.
    } catch (std::runtime_error& re) {
      std::cout << fmt::format("Runtime error: {}\n", re.what());
      break;
    }
  }

  ai.clearHistory();
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
  attemptsRemaining = 3;
  while (attemptsRemaining) {
    try {
      json json_result;
      ai >> json_result;
      translation = json_result["Translation"].get<std::string>();
      std::cout << fmt::format("[SQLwrite] translation of SQL query back to natural language: {}\n", translation.c_str());
      break;
    } catch (nlohmann::json_abi_v3_11_2::detail::parse_error& pe) {
      // Retry if there were JSON parse errors.
    } catch (nlohmann::json_abi_v3_11_2::detail::type_error& te) {
      // Retry if there were JSON parse errors.
    } catch (fmt::v9::format_error& fe) {
      std::cerr << "error: " << fe.what() << std::endl;
      // Retry if there is a format error.
    } catch (std::runtime_error& re) {
      std::cout << fmt::format("Runtime error: {}\n", re.what());
      break;
    }
    attemptsRemaining--;
  }
  
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
