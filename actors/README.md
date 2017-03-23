# Adding actors

## C actors

Actors written in C are located in the actors folder and are included in a runtime by adding them to the actor_types definition in actor.c. An actor should implement functions part of the actor_t structure in actor.h, the existing actors serves as examples, and see:

- fifo.h for functions to access port data
- token.h for functions to encode and decode token data

## Python actors

Python actors are located in the libmpy/modules folder and all actors located in that folder are included as frozen modules when building the MicroPython library. The default configuration of MicroPython enables a limited set of functionality in MicroPython, adding new actors can imply enabling functionality in libmpy/mpconfigport.h to function.
