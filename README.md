# SQLwrite

by [Emery Berger](https://emeryberger.com)

Integrates AI into your database: automatically converts
natural language queries into SQL, and then runs the SQL query.  As
far as we are aware, this is the first integration of natural language
queries into a production database manager. Currently works as an
extension to SQLite3, more to come!


## Examples

### Basic query synthesis

```
% ./sqlite3 test/test.db
SQLite version 3.41.2 2023-03-22 11:56:21
Enter ".help" for usage hints.
sqlite> .load sqlwrite
SQLwrite extension successfully initialized. Please report any issues to https://github.com/plasma-umass/sqlwrite/issues/new
sqlite> select ask('give me a list of all artist names that contain the letter y.');
SQL query: SELECT ArtistName FROM Artists WHERE ArtistName LIKE '%y%';
Paul McCartney
The Tea Party
Wayne Jury
Strapping Young Lad
Slayer
Pat Metheny
```

### Large databases, synthesizes JOINs

Using a [large SQLite database](https://github.com/lerocha/chinook-database/blob/master/ChinookDatabase/DataSources/Chinook_Sqlite.sqlite):

```
% ./sqlite3 Chinook_Sqlite.sqlite
sqlite> select ask('show me the total invoiced for all artists.');
{SQLwrite translation: SELECT SUM(Total) FROM Invoice;}
2328.6

sqlite> select ask('show me the total invoiced for all artists whose last name starts with "S"');
{SQLwrite translation: SELECT SUM(Invoice.Total) FROM Invoice JOIN Customer ON Invoice.CustomerId = Customer.CustomerId WHERE Customer.LastName LIKE 'S%';}
306.98
```

### Natural languages besides English!

```
sqlite> select ask('Haz una lista de todos los artistas cuyos nombres empiezan con la letra L');
{SQLwrite translation: SELECT Name FROM Artist WHERE Name LIKE 'L%';}
Led Zeppelin
Luiz Melodia
Legião Urbana
Lenny Kravitz
[...]
```

## Installation

Download and run `make`. Currently Mac and Linux only. You may need to install the `curl` library.

### Ubuntu

```
sudo apt install libcurl4-gnutls-dev
```

## Usage

Either use the built-in SQLite (if it was built to allow extensions), or run the generated `sqlite3` file on your database:

```
./sqlite3 my_database.db
```

and then run the following command to load the SQLwrite extension:

```
select load_extension("the_path_to_your_sqlwrite_directory/sqlwrite");
```

or:
```
.load the_path_to_your_sqlwrite_directory/sqlwrite
```

You can now issue English language queries by using the `ask` function:

```
SELECT ask('(whatever you want)');
```

## Acknowledgements

SQLwrite includes SQLite3 (https://www.sqlite.org/index.html), and is
built with the assistance of several excellent libraries, whose code
is (for now) included in this repository:

* https://github.com/olrea/openai-cpp
* https://github.com/nlohmann/json
* https://fmt.dev/latest/index.html

