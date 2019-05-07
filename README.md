# occi11
Reliable [Oracle C++ Call Interface](https://docs.oracle.com/en/database/oracle/oracle-database/18/adobj/oracle-c-cpp-call-interface-odci.html#GUID-5B8FDC66-51A0-425C-9666-E7D72C89E3DD) Wrapper for C++11 with Exception Handling

## Introduction

This project is proposed to deliver a reliable behaviour when executing SQL statements on Oracle databases. Bad behaviour may happen when connection is lost while in the middle of statement execution, when a custom user exception is thrown from PL/SQL code and when a database role is violated.

The code uses *lambda expressions* feature of the C++11, i.e. we put the statement we want to be executed as the first argument to functions, and put success, query and error handlers as optional arguments.

## Prerequisites

* [GNU Build System](https://en.wikipedia.org/wiki/GNU_Build_System)
* [Oracle Instant Client Package - Basic](http://www.oracle.com/technetwork/topics/linuxx86-64soft-092277.html)
* [Oracle Instant Client Package - SDK](http://www.oracle.com/technetwork/topics/linuxx86-64soft-092277.html)
