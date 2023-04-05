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

std::string removeEscapedNewlines(const std::string& s) {
    std::string result = s;
    std::size_t pos = result.find("\\n");
    while (pos != std::string::npos) {
        result.erase(pos, 2);
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
  sqlite3_prepare_v2(db, "SELECT name, sql FROM sqlite_master WHERE type='table' OR type='view'", -1, &stmt, NULL);

  auto query = fmt::format("Given a database with the following tables and schemas, write a SQL query in SQLite's SQL dialect that answers this question or produces the desired report: '{}'. Only produce the SQL query, surrounded in backquotes (```).\n\nSchemas:\n", (const char *) sqlite3_value_text(argv[0]));
  
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    const char *sql = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    query += fmt::format("Schema for {}: {}\n", name, sql);
  }

  nlohmann::json prompt;
  prompt["model"] = "gpt-3.5-turbo";
  nlohmann::json chat_messages;
  chat_messages["role"] = "user";
  chat_messages["content"] = query;
  prompt["messages"].push_back(chat_messages);

  auto prompt_str = prompt.dump();

  try {
    auto chat = openai::chat().create(prompt); // prompt_str.c_str());
    auto result = chat["choices"][0]["message"]["content"].dump(2);
    std::string output;

    auto start = result.find("```") + 3; // add 3 to skip over the backquotes
    auto end = result.rfind("```"); // find the last occurrence of backquotes
    output = result.substr(start, end - start); // extract the substring between the backquotes
    // Remove any escaped newlines.
    output = removeEscapedNewlines(output);
    // Add a semicolon.
    output += ";";
    std::cout << "{SQLwrite translation: " << output.c_str() << "}\n";

    // Send the query to the database.
    auto rc = sqlite3_exec(db, output.c_str(), print_em, nullptr, nullptr);

    if (rc != SQLITE_OK) {
      std::cout << "Error executing SQL statement: " << sqlite3_errmsg(db) << std::endl;
      return;
    }
    
  } catch (std::runtime_error re) {
    std::cout << "Runtime error: " << re.what() << std::endl;
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
