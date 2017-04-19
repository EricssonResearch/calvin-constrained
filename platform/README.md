# Porting to a new platform

Platform implementations are located in separate subfolders and should:

- Contain a README.md with instructions on how to build and use the platform port.
- Implement the functions defined in platform.h, these functions are the interface used by the constrained runtime to setup and access platform functions.
