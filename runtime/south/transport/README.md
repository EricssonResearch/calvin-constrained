# Create a new transport client

Transport clients are located in separate subfolders and should:

- Implement the functions part of the cc_transport_client_t structure defined in cc_transport.h.
- Implement a function to create the cc_transport_client_t object and add it to the CC_TRANSPORTS preprocessor define.
