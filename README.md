# SQLwrite

by [Emery Berger](https://emeryberger.com)

Integrates AI into your database: automatically converts English-language queries into SQL, and then runs the SQL query.
As far as we are aware, this is the first integration of natural language queries into a database manager (please
let us know if this is not the case!).


## Example

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

## Installation

Download and run `make`. Currently Mac only. More to come.

## Usage

Run the generated `sqlite3` file, and then run the following command to load the SQLwrite extension:

```
.load sqlwrite
```

You can now issue English language queries by using the `ask` function:

```
SELECT ask('(whatever you want)');
```
