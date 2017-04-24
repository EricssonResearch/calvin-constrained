### Enable TLS

Place the certificate, cert.pem, and the private key, private.key, in the root of the calvin-constrained repository and enable TLS with:

```
CFLAGS += -DUSE_TLS
INC_PATHS  = -Imbedtls/include
```

And build and include mbedtls library or include the following source files in the build:

```
C_SOURCE_FILES += \
mbedtls/library/aes.c \
mbedtls/library/aesni.c \
mbedtls/library/arc4.c \
mbedtls/library/asn1parse.c \
mbedtls/library/asn1write.c \
mbedtls/library/base64.c \
mbedtls/library/bignum.c \
mbedtls/library/blowfish.c \
mbedtls/library/camellia.c \
mbedtls/library/ccm.c \
mbedtls/library/certs.c \
mbedtls/library/cipher.c \
mbedtls/library/cipher_wrap.c \
mbedtls/library/ctr_drbg.c \
mbedtls/library/debug.c \
mbedtls/library/des.c \
mbedtls/library/dhm.c \
mbedtls/library/ecdh.c \
mbedtls/library/ecdsa.c \
mbedtls/library/ecp.c \
mbedtls/library/ecp_curves.c \
mbedtls/library/entropy.c \
mbedtls/library/entropy_poll.c \
mbedtls/library/error.c \
mbedtls/library/gcm.c \
mbedtls/library/havege.c \
mbedtls/library/hmac_drbg.c \
mbedtls/library/md.c \
mbedtls/library/md2.c \
mbedtls/library/md4.c \
mbedtls/library/md5.c \
mbedtls/library/md_wrap.c \
mbedtls/library/memory_buffer_alloc.c \
mbedtls/library/oid.c \
mbedtls/library/padlock.c \
mbedtls/library/pem.c \
mbedtls/library/pk.c \
mbedtls/library/pk_wrap.c \
mbedtls/library/pkcs11.c \
mbedtls/library/pkcs12.c \
mbedtls/library/pkcs5.c \
mbedtls/library/pkparse.c \
mbedtls/library/pkwrite.c \
mbedtls/library/platform.c \
mbedtls/library/ripemd160.c \
mbedtls/library/rsa.c \
mbedtls/library/sha1.c \
mbedtls/library/sha256.c \
mbedtls/library/sha512.c \
mbedtls/library/ssl_cache.c \
mbedtls/library/ssl_ciphersuites.c \
mbedtls/library/ssl_cli.c \
mbedtls/library/ssl_cookie.c \
mbedtls/library/ssl_srv.c \
mbedtls/library/ssl_ticket.c \
mbedtls/library/ssl_tls.c \
mbedtls/library/threading.c \
mbedtls/library/version.c \
mbedtls/library/version_features.c \
mbedtls/library/x509.c \
mbedtls/library/x509_create.c \
mbedtls/library/x509_crl.c \
mbedtls/library/x509_crt.c \
mbedtls/library/x509_csr.c \
mbedtls/library/xtea.c \
mbedtls/library/timing.c \
crypto/crypto.c
```
