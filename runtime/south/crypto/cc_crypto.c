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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cc_crypto.h"
#include "../platform/cc_platform.h"
#include "../../north/cc_transport.h"

#define CRYPTO_PARSE_BUF_SIZE 200
#define CRYPTO_CERT_FILE "cert.pem"
#define CRYPTO_PRIVE_KEY_FILE "private.key"

result_t crypto_get_node_info(char domain[], char name[], char id[])
{
  char buf[CRYPTO_PARSE_BUF_SIZE];
  mbedtls_x509_crt crt;
  result_t result = FAIL;
  int ret = 0;

#ifdef MBEDTLS_NO_PLATFORM_ENTROPY
  mbedtls_platform_set_calloc_free(platform_mem_calloc, platform_mem_free);
#endif

  memset(buf, 0, CRYPTO_PARSE_BUF_SIZE);
  mbedtls_x509_crt_init(&crt);

  ret = mbedtls_x509_crt_parse_file(&crt, CRYPTO_CERT_FILE);
  if (ret != 0) {
    log_error("Failed to load certificate %d '-0x%x'", ret, ret);
    return FAIL;
  }

  if (mbedtls_x509_dn_gets((char *)buf, sizeof(buf) - 1, &crt.subject) > 0) {
    if (sscanf(buf, "O=%[^,], dnQualifier=%[^,], CN=%s", domain, id, name) == 3)
      result = SUCCESS;
	  else
      log_error("Failed to parse certificate subject");
  }

  mbedtls_x509_crt_free(&crt);

  return result;
}

static void crypto_debug_cb(void *ctx, int level, const char *file, int line, const char *str)
{
  ((void)ctx);
  ((void)level);

  log_debug("%s:%04d: %s", file, line, str);
}

static int _crypto_tls_send(void *ctx, const unsigned char *buffer, size_t size)
{
  transport_client_t *transport_client = (transport_client_t *)ctx;

  return transport_client->send(transport_client, (char *)buffer, size);
}

static int _crypto_tls_read(void *ctx, unsigned char *buffer, size_t size)
{
  transport_client_t *transport_client = (transport_client_t *)ctx;

  return transport_client->recv(transport_client, (char *)buffer, size);
}

result_t crypto_tls_init(char *node_id, transport_client_t *transport_client)
{
  int ret = 0;

  mbedtls_ssl_init(&transport_client->crypto.ssl);
  mbedtls_ssl_config_init(&transport_client->crypto.conf);
  mbedtls_x509_crt_init(&transport_client->crypto.cacert);
  mbedtls_pk_init(&transport_client->crypto.private_key);

#ifndef MBEDTLS_NO_PLATFORM_ENTROPY
  mbedtls_ctr_drbg_init(&transport_client->crypto.ctr_drbg);
  mbedtls_entropy_init(&transport_client->crypto.entropy);

  if (mbedtls_ctr_drbg_seed(&transport_client->crypto.ctr_drbg, mbedtls_entropy_func, &transport_client->crypto.entropy, (const unsigned char *)node_id, strlen(node_id)) != 0) {
	  log_error("Failed to init random number generator");
	  return FAIL;
  }
#endif

  ret = mbedtls_x509_crt_parse_file(&transport_client->crypto.cacert, CRYPTO_CERT_FILE);
  if (ret < 0) {
    log_error("Failed to load certificate");
    return FAIL;
  }

  if (mbedtls_pk_parse_keyfile(&transport_client->crypto.private_key, CRYPTO_PRIVE_KEY_FILE, NULL) < 0) {
    log_error("Failed to load private key");
    return FAIL;
  }

  if (mbedtls_ssl_config_defaults(&transport_client->crypto.conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
    log_error("Failed to setup SSL/TLS structure");
    return FAIL;
  }

  mbedtls_ssl_conf_authmode(&transport_client->crypto.conf, MBEDTLS_SSL_VERIFY_REQUIRED);
  mbedtls_ssl_conf_ca_chain(&transport_client->crypto.conf, &transport_client->crypto.cacert, NULL);

#ifndef MBEDTLS_NO_PLATFORM_ENTROPY
  mbedtls_ssl_conf_rng(&transport_client->crypto.conf, mbedtls_ctr_drbg_random, &transport_client->crypto.ctr_drbg);
#else
  mbedtls_ssl_conf_rng(&transport_client->crypto.conf, platform_random_vector_generate, NULL);
#endif
  mbedtls_ssl_conf_dbg(&transport_client->crypto.conf, crypto_debug_cb, NULL);
  mbedtls_ssl_conf_own_cert(&transport_client->crypto.conf, &transport_client->crypto.cacert, &transport_client->crypto.private_key);

  if (mbedtls_ssl_setup(&transport_client->crypto.ssl,
    &transport_client->crypto.conf) != 0) {
      log_error("Failed setup SSL/TLS");
      return FAIL;
  }

  mbedtls_ssl_set_bio(&transport_client->crypto.ssl, (void *)transport_client, _crypto_tls_send, _crypto_tls_read, NULL);

  while ((ret = mbedtls_ssl_handshake(&transport_client->crypto.ssl)) != 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      log_error("Failed to perform SSL/TLS handshake '%d'", ret);
      return FAIL;
    }
  }

  if (mbedtls_ssl_get_verify_result(&transport_client->crypto.ssl) != 0) {
    log_error("Failed to verify certificate");
    return FAIL;
  }

  log("TLS initialized");

  return SUCCESS;
}

int crypto_tls_send(transport_client_t *transport_client, char *buffer, size_t size)
{
  int ret = 0;

  while ((ret = mbedtls_ssl_write(&transport_client->crypto.ssl, (const unsigned char *)buffer, size)) <= 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      log_error("mbedtls_ssl_write returned %d", ret);
      break;
	  }
  }

  return ret;
}

int crypto_tls_recv(transport_client_t *transport_client, char *buffer, size_t size)
{
  return mbedtls_ssl_read(&transport_client->crypto.ssl, (unsigned char *)buffer, size);
}

void crypto_tls_free(crypto_t *crypto)
{
  mbedtls_ssl_close_notify(&crypto->ssl);
  mbedtls_x509_crt_free(&crypto->cacert);
  mbedtls_pk_free(&crypto->private_key);
  mbedtls_ssl_free(&crypto->ssl);
  mbedtls_ssl_config_free(&crypto->conf);
#ifndef MBEDTLS_NO_PLATFORM_ENTROPY
  mbedtls_ctr_drbg_free(&crypto->ctr_drbg);
  mbedtls_entropy_free(&crypto->entropy);
#endif
}
