# Quantica
[![Build Status](https://travis-ci.org/yorickdewid/Quantica.svg)](https://travis-ci.org/yorickdewid/Quantica)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/6570/badge.svg)](https://scan.coverity.com/projects/yorickdewid-quantica)

A fast and scalable object database designed to store large amounts of both structured and unstructured data.
Quantica is modular, polymorfistic and able to build relationships between all kind of data structures.
There is no genetic data architecture, structure or storage convention but rather an implicit model based on and defined by the existing data.

## What can it do?
Quantica can store unstructured, NoSQL data into records and retrieve them in a structured and relational way using a query language, or vice versa.
This query language is partly derived from SQL, although it does not fully comply with the SQL standards.
The core relies on an unique storage model that makes it possible to query data in various ways.
All communication is handled by a WebAPI which can access the data directly (API calls) or through the SQL interface.
Both interfaces provide the same features, but through a different request method.

## Features
- Fast storage operations (get, update, delete)
- Store data regardless of type, format and size
- Event-driven / non-blocking communication
- Multiple interfaces: WebAPI and SQL
- Multiple data formats (raw files, arrays, key-value, objects, tables)

## Quantica is __not__
- A full RDBMS (but does support SQL, relations, indexes and tables)
- A full NoSQL database (but does support such functions and operations)
- A data migration tool (but could very well serve as such with little creativity)
- A data cluster / distributed data processor (planned for next release)

## Roadmap
See the [TODO list](TODO.md)

## License

BSD &copy; 2015 Quantica
