# SQLwrite

by [Emery Berger](https://emeryberger.com)

Integrates AI into your database: automatically converts
natural language queries into SQL, and then runs the SQL query.  As
far as we are aware, this is the first integration of natural language
queries into a production database manager. Currently works as an
extension to SQLite3, more to come!


## Examples

These example queries use a [large SQLite database with multiple tables](https://github.com/lerocha/chinook-database/blob/master/ChinookDatabase/DataSources/Chinook_Sqlite.sqlite):

### Basic queries

```
% ./sqlite3 Chinook_Sqlite.sqlite
sqlite> select ask('show me the total invoiced for all artists.');
2328.6
(SQLwrite translation to SQL: SELECT sum(Invoice.Total) FROM Invoice;;)
```

### Queries with JOINs

```
sqlite> select ask('show me the total invoiced for all artists whose last name starts with "S"');
306.98
(SQLwrite translation to SQL: SELECT SUM(Invoice.Total) as total_invoiced    FROM Invoice        JOIN Customer ON Invoice.CustomerId = Customer.CustomerId    WHERE Customer.LastName LIKE 'S%';)
```

### Complex query synthesis with multiple JOINs

```
sqlite> select ask('give me a list of all artists (with no duplicates) whose genre is reggae');
Cidade Negra
Lenny Kravitz
UB40
(SQLwrite translation to SQL: SELECT DISTINCT Artist.Name     FROM Artist    JOIN Album ON Artist.ArtistId = Album.ArtistId    JOIN Track ON Album.AlbumId = Track.AlbumId    JOIN Genre ON Track.GenreId = Genre.GenreId    WHERE Genre.Name = 'Reggae';;)
```

### Natural languages besides English!

```
sqlite> select ask('Haz una lista de todos los artistas cuyos nombres empiezan con la letra L');
Led Zeppelin
Luiz Melodia
Legião Urbana
Lenny Kravitz
Lulu Santos
Lost
Los Lonely Boys
Los Hermanos
Luciana Souza/Romero Lubambo
London Symphony Orchestra & Sir Charles Mackerras
Luciano Pavarotti
Leonard Bernstein & New York Philharmonic
Les Arts Florissants & William Christie
(SQLwrite translation to SQL: SELECT Name FROM Artist WHERE Name LIKE 'L%';)
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

