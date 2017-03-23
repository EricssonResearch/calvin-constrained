# Create a new transport client

Transport clients handles the communication with the runtime acting as a proxy and at least one transport client must be included in the build.

A transport client, transport_client_t, is created in transport_create() in transport.c if the uri matches the schema of the transport client. New transport clients should be implemented in the transport/ directory and should implement a function to create and initialize transport_client_t object.
