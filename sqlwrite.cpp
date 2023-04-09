/*

  SQLwrite
 
  by Emery Berger <https://emeryberger.com>

  Translates English-language queries to SQL.
  
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include <fmt/core.h>

#include "openai.hpp"
#include "sqlite3ext.h"

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

     
static void ask_command(sqlite3_context *ctx, int argc, sqlite3_value **argv) {

  if (argc != 1) {
    sqlite3_result_error(ctx, "The 'ask' command takes exactly one argument.", -1);
  }

  // Print all loaded database schemas
  sqlite3 *db = sqlite3_context_db_handle(ctx);
  sqlite3_stmt *stmt;

  auto query = fmt::format("Given a database with the following tables, schemas, and indexes, write a SQL query in SQLite's SQL dialect that answers this question or produces the desired report: '{}'. Produce a JSON object with the SQL query as a field \"SQL\". Offer any suggestions for indexing to improve query performance in a field \"Indexing\", where each entry consists of the table name and a list of columns to be indexed. Only produce output that can be parsed as JSON.\n\nSchemas:\n", (const char *) sqlite3_value_text(argv[0]));
  
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
      query += "\n\nIndexes:\n";
      printed_index_header = true;
    }
    const char *tbl_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    const char *sql = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    query += fmt::format("Index for {}: {}\n", tbl_name, sql);
  }

  nlohmann::json prompt;
  prompt["model"] = "gpt-3.5-turbo";
  nlohmann::json chat_message;
  chat_message["role"] = "assistant";
  chat_message["content"] = "I am a programming assistant who is an expert in generating SQL queries from natural language. I ONLY respond with JSON objects.";
  prompt["messages"].push_back(chat_message);
  chat_message["role"] = "user";
  chat_message["content"] = query;
  prompt["messages"].push_back(chat_message);
 
  auto prompt_str = prompt.dump();

  // std::cout << prompt_str << std::endl;
  
  int attemptsRemaining = 3;
  while (attemptsRemaining) {
    try {
      auto chat = openai::chat().create(prompt); // prompt_str.c_str());
      /// nlohmann::json result = nlohmann::json::parse("{\n    \"SQL\": \"SELECT ArtistId FROM Artists WHERE ArtistName = 'Primus';\",\n    \"Indexing\": {\n        \"Artists\": [\"ArtistName\"]\n    }\n}");
      auto result = chat["choices"][0]["message"]["content"].get<std::string>();
      // std::cout << "attempting to parse." << std::endl;
      auto json_result = nlohmann::json::parse(result);
      // std::cout << json_result.dump() << std::endl;
      
      std::string output;

      //std::cout << "SQL:" << std::endl;
      //std::cout << json_result["SQL"].get<std::string>() << std::endl;
      //std::cout << "Indexing:" << std::endl;
      //std::cout << json_result["Indexing"].dump() << std::endl;

      // TODO: subtract set of indexing results from already-indexed columns, and present to user as suggestions
      
      output = json_result["SQL"].get<std::string>();
      // Remove any escaped newlines.
      output = removeEscapedNewlines(output);
      output = removeEscapedCharacters(output);

      // Send the query to the database.
      auto rc = sqlite3_exec(db, output.c_str(), print_em, nullptr, nullptr);
      
      if (rc != SQLITE_OK) {
	if (attemptsRemaining == 0) {
	  std::cout << "Error executing SQL statement: " << sqlite3_errmsg(db) << std::endl;
	  sqlite3_finalize(stmt);
	  return;
	}
	attemptsRemaining--;
	continue;
      }
      std::cout << "(SQLwrite translation to SQL: " << output.c_str() << ")\n";
      attemptsRemaining = 0;
    } catch (std::runtime_error re) {
      std::cout << "Runtime error: " << re.what() << std::endl;
    } catch (nlohmann::json_abi_v3_11_2::detail::parse_error pe) {
      // Retry if there were JSON parse errors.
    }
  }
  sqlite3_finalize(stmt);
}



extern "C" int sqlite3_sqlwrite_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi)
{
  openai::start();
  SQLITE_EXTENSION_INIT2(pApi);
    
  int rc = sqlite3_create_function(db, "ask", -1, SQLITE_UTF8, db, &ask_command, NULL, NULL);
  if (rc != SQLITE_OK) {
    *pzErrMsg = sqlite3_mprintf("Failed to create ask function: %s", sqlite3_errmsg(db));
    return rc;
  }
  printf("SQLwrite extension successfully initialized. Please report any issues to https://github.com/plasma-umass/sqlwrite/issues/new\n");

  return SQLITE_OK;
}
