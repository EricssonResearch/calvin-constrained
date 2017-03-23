# Porting to a new platform

The platform specific parts, such as initializing the platform and accessing hardware resources from the calvin-constrained core, are isolated from the core functionality. Platform ports are located in the platform/ directory and should contain:

- a README.md with instructions on how to use the platform port
- a Makefile for building to the specific architecture
- a implementation of the functions defined in platform.h

The functions defined in platform.h servers as a interface used by the calvin-constrained core to setup and access platform functions.
