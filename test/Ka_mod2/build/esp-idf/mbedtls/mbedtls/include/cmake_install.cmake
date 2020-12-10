# Install script for directory: Z:/esp-idf/components/mbedtls/mbedtls/include

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/Ka-Radio32")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mbedtls" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/aes.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/aesni.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/arc4.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/aria.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/asn1.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/asn1write.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/base64.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/bignum.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/blowfish.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/bn_mul.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/camellia.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ccm.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/certs.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/chacha20.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/chachapoly.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/check_config.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/cipher.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/cipher_internal.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/cmac.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/compat-1.3.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/config.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ctr_drbg.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/debug.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/des.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/dhm.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ecdh.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ecdsa.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ecjpake.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ecp.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ecp_internal.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/entropy.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/entropy_poll.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/error.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/gcm.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/havege.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/hkdf.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/hmac_drbg.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/md.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/md2.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/md4.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/md5.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/md_internal.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/memory_buffer_alloc.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/net.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/net_sockets.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/nist_kw.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/oid.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/padlock.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pem.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pk.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pk_internal.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pkcs11.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pkcs12.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/pkcs5.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/platform.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/platform_time.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/platform_util.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/poly1305.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ripemd160.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/rsa.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/rsa_internal.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/sha1.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/sha256.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/sha512.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl_cache.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl_ciphersuites.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl_cookie.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl_internal.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/ssl_ticket.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/threading.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/timing.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/version.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/x509.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/x509_crl.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/x509_crt.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/x509_csr.h"
    "Z:/esp-idf/components/mbedtls/mbedtls/include/mbedtls/xtea.h"
    )
endif()

