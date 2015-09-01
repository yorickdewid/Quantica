# Quantica [![Build Status](https://travis-ci.org/yorickdewid/Quantica.svg)](https://travis-ci.org/yorickdewid/Quantica)

A fast and scalable object database designed to store large amounts of structured and unstructured data.
Quantica is one of the few databases that is both modular, polymorfistic and is able to build relationships between all kind of data structures.
There is no genetic data architecture, structure or storage convention but rather an implicit model based on and defined by the existing data.

## What can it do?
Quantica can store unstructured, NoSQL (big)data into records and retrieve them in a structured and relational way using a query language, or vice versa.
This query language is partly derived from SQL, although it does not fully comply with the SQL standards.
The core relies on an unique storage model that makes it possible to query data in various ways.
All communication is handled by a WebAPI which can access the data directly (API calls) or through the SQL interface.
Both interfaces provide the same features, but through a different syntax.

## Features
- Fast data operations (get, update, delete)
- Globally unique data identifier
- Store data regardless of type, format and size
- Event-driven / non-blocking communication
- Multiple interfaces: WebAPI and SQL
- Multiple data formats (raw files, arrays, key-value, objects, tables)

## Quantica is __not__
- Full RDBMS (but does support SQL, relations, indexes and tables)
- Full NoSQL database (but does support most bigdata functions and operations)
- Data migration software (but could very well serve as such with little creativity)
- Data cluster / distributed data processor (planned for next release)

## Roadmap
See the [TODO list](TODO.md)

## License

BSD &copy; 2015 Quantica

