# C actors

Actors written in C are located in the actors folder and are included in a runtime by adding them to the actor_types definition in actor.c. An actor should implement functions part of the actor_t structure in actor.h, the existing actors serves as examples, and see:

- fifo.h for functions to access port data
- token.h for functions to encode and decode token data
