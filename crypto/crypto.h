/*
 * Copyright (c) 2016 Ericsson AB
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef CRYPTO_H
#define CRYPTO_H

#include "mbedtls/ssl.h"
#ifndef MBEDTLS_NO_PLATFORM_ENTROPY
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#endif
#include "../common.h"

struct node_t;
struct transport_client_t;

typedef struct crypto_t {
#ifndef MBEDTLS_NO_PLATFORM_ENTROPY
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
#endif
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_x509_crt cacert;
  mbedtls_pk_context private_key;
} crypto_t;

result_t crypto_get_node_info(char domain[], char name[], char id[]);
result_t crypto_tls_init(char *node_id, struct transport_client_t *transport_client);
int crypto_tls_send(struct transport_client_t *transport_client, char *buffer, size_t size);
int crypto_tls_recv(struct transport_client_t *transport_client, char *buffer, size_t size);
void crypto_tls_free(crypto_t *crypto);

#endif /* CRYPTO_H */
