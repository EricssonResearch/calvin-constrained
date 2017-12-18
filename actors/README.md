# C actors

Actors should implement functions part of the actor_t structure in cc_actor.h and are included in the firmware by adding them to the CC_C_ACTORS preprocessor define. Use the existing actors as examples, and see:

- cc_fifo.h for functions to access port data
- cc_token.h for functions to encode and decode token data
