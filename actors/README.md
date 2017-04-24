# C actors

Actors should implement functions part of the actor_t structure in cc_actor.h and are included by adding them to the actor_types definition in cc_actor.c. Use the existing actors as examples, and see:

- cc_fifo.h for functions to access port data
- cc_token.h for functions to encode and decode token data
