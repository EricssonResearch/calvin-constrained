# Create a new transport client

Transport clients are located in separate subfolders and should:

- Implement the functions part of the cc_transport_client_t structure defined in cc_transport.h.
- Expose a function to create a cc_transport_client_t object configured for the specific transport client. This function should be called from transcc_port_create in cc_transport.c if the URI matches the schema of the transport client.
