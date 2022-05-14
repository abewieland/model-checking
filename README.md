## M++

This repository contains M++, a model checker for distributed systems written by
Jordan Barkin and Abe Wieland for CS 260r at Harvard.

# Files
`model.hpp` contains the machine and message abstractions extended by models, as
well as some useful header files and other structures for model checking, like
the system state and model objects. Most models should only have to import
`model.hpp`

`model.cpp` contains the actual model checking routine, with the symmetry and
guided search optimizations.

`ack.cpp`, `example.cpp`, `paxos.cpp`, and `replication.cpp` are models to be
checked.

# Running
Each of the models to be checked will compile to its own file via `make`. Some
have command line options that can be introspected via the `-h` flag. Some of
the models also have bugs builtin that are disabled by deafult, but can be
enabled if built using `make B=1`.
