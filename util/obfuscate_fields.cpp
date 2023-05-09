#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
    
    // Open the database file
    rc = sqlite3_open("chi.sqlite", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    // Get the list of tables in the database
    sqlite3_stmt *stmt;
    const char *query = "SELECT name FROM sqlite_master WHERE type='table'";
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    // Iterate over each table in the database
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *table_name = (const char *) sqlite3_column_text(stmt, 0);
        char *rename_query = new char[1024];
        sprintf(rename_query, "ALTER TABLE %s RENAME TO %s_temp", table_name, table_name);
        rc = sqlite3_exec(db, rename_query, NULL, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            sqlite3_close(db);
            return 1;
        }

        // Get the list of columns in the table
        sqlite3_stmt *stmt2;
        const char *query2 = "PRAGMA table_info(%s_temp)";
        char *query2_formatted = new char[1024];
        sprintf(query2_formatted, query2, table_name);
        rc = sqlite3_prepare_v2(db, query2_formatted, -1, &stmt2, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return 1;
        }

        // Iterate over each column in the table
        while (sqlite3_step(stmt2) == SQLITE_ROW) {
            const char *column_name = (const char *) sqlite3_column_text(stmt2, 1);
            const char *column_type = (const char *) sqlite3_column_text(stmt2, 2);
            int column_pk = sqlite3_column_int(stmt2, 5);
            char *new_column_name = new char[1024];
            sprintf(new_column_name, "F%d", sqlite3_column_int(stmt2, 0));

            char *alter_query = new char[1024];
            sprintf(alter_query, "ALTER TABLE %s_temp RENAME COLUMN %s TO %s",
                    table_name, column_name, new_column_name);
            rc = sqlite3_exec(db, alter_query, NULL, 0, &zErrMsg);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
                sqlite3_close(db);
                return 1;
            }

            // Update any foreign keys that reference the column
            if (!column_pk) {
                sqlite3_stmt *stmt3;
		const char *query3 = "SELECT name, sql FROM sqlite_master WHERE type='table' AND sql LIKE '%%REFERENCES %s_temp(%s)%%'";
		char *query3_formatted = new char[1024];
		sprintf(query3_formatted, query3, table_name, column_name);
		rc = sqlite3_prepare_v2(db, query3_formatted, -1, &stmt3, NULL);
		if (rc != SQLITE_OK) {
		  fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
		  sqlite3_close(db);
		  return 1;
		}

		while (sqlite3_step(stmt3) == SQLITE_ROW) {
		  const char *ref_table_name = (const char *) sqlite3_column_text(stmt3, 0);
		  const char *ref_sql = (const char *) sqlite3_column_text(stmt3, 1);
		  char *new_ref_sql = new char[1024];
		  sprintf(new_ref_sql, "%s(%s)", ref_table_name, new_column_name);
		  
		  char *update_query = new char[1024];
		  sprintf(update_query, "UPDATE sqlite_master SET sql='%s' WHERE name='%s'",
			  ref_sql, ref_table_name);
		  rc = sqlite3_exec(db, update_query, NULL, 0, &zErrMsg);
		  if (rc != SQLITE_OK) {
                    fprintf(stderr, "SQL error: %s\n", zErrMsg);
                    sqlite3_free(zErrMsg);
                    sqlite3_close(db);
                    return 1;
		  }
		}
		
		sqlite3_finalize(stmt3);
		delete [] query3_formatted;
	    }
	    
	    delete [] new_column_name;
	    delete [] alter_query;
	}
	
	delete [] query2_formatted;
	sqlite3_finalize(stmt2);
	
	char *rename_query2 = new char[1024];
	sprintf(rename_query2, "ALTER TABLE %s_temp RENAME TO %s", table_name, table_name);
	rc = sqlite3_exec(db, rename_query2, NULL, 0, &zErrMsg);
	if (rc != SQLITE_OK) {
	  fprintf(stderr, "SQL error: %s\n", zErrMsg);
	  sqlite3_free(zErrMsg);
	  sqlite3_close(db);
	  return 1;
	}
	
	delete [] rename_query2;
	delete [] rename_query;
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}
