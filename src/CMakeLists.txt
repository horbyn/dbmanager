if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  message(STATUS "x86_64 架构")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
  message(STATUS "aarch64 架构")
else()
  message(FATAL_ERROR "未知架构: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(DBMNGR_ENABLE_STATIC_LIB)
  set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
  find_library(MYSQL_STATIC_LIB libmysqlclient.a PATHS /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
  find_library(MHD_STATIC_LIB libmicrohttpd.a PATHS /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
  find_library(SSL_STATIC_LIB libssl.a PATHS /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
  find_library(CRYPTO_STATIC_LIB libcrypto.a PATHS /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
  find_library(ZSTD_STATIC_LIB libzstd.a PATHS /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
  find_library(GNUTLS_STATIC_LIB libgnutls.a PATHS /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
  find_library(TASN1_STATIC_LIB libtasn1.a PATHS /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
  find_library(HOGWEED_STATIC_LIB libhogweed.a PATHS /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
  find_library(NETTLE_STATIC_LIB libnettle.a PATHS /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
  find_library(GMP_STATIC_LIB libgmp.a PATHS /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
  find_library(Z_STATIC_LIB libz.a PATHS /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
  find_library(NGHTTP2_STATIC_LIB libnghttp2.a PATHS /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
  find_library(PSL_STATIC_LIB libpsl.a PATHS /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
  find_library(IDM2_STATIC_LIB libidn2.a PATHS /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
  find_library(UNISTRING_STATIC_LIB libunistring.a PATHS /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
  find_library(FFI_STATIC_LIB libffi.a PATHS /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
else()
  message(FATAL_ERROR "暂不支持动态链接")
endif()

file(GLOB sources "*.c")
add_library(src
  STATIC
  ${sources}
)
if(DBMNGR_ENABLE_STATIC_LIB)
  target_link_libraries(src
    PRIVATE
    ${DBMNGR_DEPS_BUILD_DIR}/zlog/lib/libzlog.a
    ${MYSQL_STATIC_LIB}
    ${MHD_STATIC_LIB}
    ${CURL_LIBRARY}
    ${SSL_STATIC_LIB}
    ${CRYPTO_STATIC_LIB}
    ${ZSTD_STATIC_LIB}
    ${GNUTLS_STATIC_LIB}
    ${TASN1_STATIC_LIB}
    ${HOGWEED_STATIC_LIB}
    ${NETTLE_STATIC_LIB}
    ${GMP_STATIC_LIB}
    ${Z_STATIC_LIB}
    ${NGHTTP2_STATIC_LIB}
    ${PSL_STATIC_LIB}
    ${IDM2_STATIC_LIB}
    ${UNISTRING_STATIC_LIB}
    ${FFI_STATIC_LIB}
    -lp11-kit # 只有动态库
    -lstdc++  # mysql client 包含了 C++ 符号，还必须链接 C++ 标准库
  )
else()
  message(FATAL_ERROR "暂不支持动态链接")
endif()
