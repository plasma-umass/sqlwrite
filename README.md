# SQLwrite

by [Emery Berger](https://emeryberger.com)

Integrates AI into your database: automatically converts natural
language queries into SQL, and then runs the SQL query.  As far as we
are aware, this is the first integration of LLMs to enable natural
language queries into a production database manager. Currently works
as an extension to SQLite3 (more to come). In addition to generating
queries, SQLwrite also produces suggestions to improve query
performance (e.g., creating new indices).

*NOTE*: To use SQLwrite, you must first set up an OpenAI API key. If you
already have an API key, you can set it as an environment variable
called `OPENAI_API_KEY`. If you do not have one yet,
you can get a key here: https://platform.openai.com/account/api-keys

```
export OPENAI_API_KEY=<your-api-key>
```

## Examples

These example queries use a [large SQLite database with multiple tables](https://github.com/lerocha/chinook-database/blob/master/ChinookDatabase/DataSources/Chinook_Sqlite.sqlite):

### Getting started

```
% export OPENAI_API_KEY=<your-api-key>
% ./sqlite3 Chinook_Sqlite.sqlite
sqlite> .load sqlwrite
SQLwrite extension successfully initialized. You can now use natural language queries like "select ask('show me all artists.');".
Please report any issues to https://github.com/plasma-umass/sqlwrite/issues/new
```

### Basic queries

```sql
sqlite> select ask('show me the total invoiced for all artists.');
2328.6
[SQLwrite] translation to SQL: SELECT SUM(Total) AS total_invoiced FROM Invoice;
[SQLwrite] indexing suggestions to improve the performance for this query:
(1): CREATE INDEX idx_invoice_total ON Invoice (Total);
(2): CREATE INDEX idx_customer_invoice ON Invoice (CustomerId);
```

### Queries with JOINs

```sql
sqlite> select ask('show me the total invoiced for all artists whose last name starts with "S"');
306.98
[SQLwrite] translation to SQL: SELECT sum(Invoice.Total) as total_invoiced FROM Invoice JOIN Customer ON Invoice.CustomerId = Customer.CustomerId WHERE Customer.LastName LIKE 'S%'
[SQLwrite] indexing suggestions to improve the performance for this query:
(1): CREATE INDEX idx_invoice_customer_lastname ON Invoice (CustomerId) WHERE (CustomerId IN (SELECT CustomerId FROM Customer WHERE LastName LIKE 'S%'))
(2): CREATE INDEX idx_customer_lastname ON Customer (LastName)
```

### Complex query synthesis with multiple JOINs

```sql
sqlite> select ask('give me a list of all artists (with no duplicates) whose genre is reggae');
Cidade Negra
Lenny Kravitz
UB40
[SQLwrite] translation to SQL: SELECT DISTINCT Artist.Name FROM Artist JOIN Album ON Album.ArtistId = Artist.ArtistId JOIN Track ON Track.AlbumId = Album.AlbumId JOIN Genre ON Track.GenreId = Genre.GenreId WHERE Genre.Name = 'Reggae';
[SQLwrite] indexing suggestions to improve the performance for this query:
(1): CREATE INDEX [IFK_TrackGenreReggae] ON [Track] ([GenreId]) WHERE Name = 'Reggae';
```

### Natural languages besides English!

```sql
sqlite> select ask('Haz una lista de todos los músicos cuyos nombres empiezan con la letra L');
22|Led Zeppelin
33|Luiz Melodia
99|Legião Urbana
100|Lenny Kravitz
101|Lulu Santos
149|Lost
162|Los Lonely Boys
187|Los Hermanos
201|Luciana Souza/Romero Lubambo
223|London Symphony Orchestra & Sir Charles Mackerras
227|Luciano Pavarotti
228|Leonard Bernstein & New York Philharmonic
258|Les Arts Florissants & William Christie
[SQLwrite] translation to SQL: SELECT * FROM Artist WHERE Name LIKE 'L%';
[SQLwrite] indexing suggestions to improve the performance for this query:
(1): CREATE INDEX [IFK_ArtistName] ON [Artist] ([Name]);
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

```sql
SELECT ask('(whatever you want)');
```

## Acknowledgements

SQLwrite includes SQLite3 (https://www.sqlite.org/index.html), and is
built with the assistance of several excellent libraries, whose code
is (for now) included in this repository:

* https://github.com/olrea/openai-cpp
* https://github.com/nlohmann/json
* https://fmt.dev/latest/index.html

