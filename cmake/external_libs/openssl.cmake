if(ENABLE_GITEE)
    set(REQ_URL "https://gitee.com/mirrors/openssl/repository/archive/OpenSSL_1_1_1k.tar.gz")
    set(SHA256 "b92f9d3d12043c02860e5e602e50a73ed21a69947bcc74d391f41148e9f6aa95")
else()
    set(REQ_URL "https://github.com/openssl/openssl/archive/refs/tags/OpenSSL_1_1_1k.tar.gz")
    set(SHA256 "b92f9d3d12043c02860e5e602e50a73ed21a69947bcc74d391f41148e9f6aa95")
endif()

set(OPENSSL_PATCH_ROOT ${CMAKE_SOURCE_DIR}/third_party/patch/openssl)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR APPLE)
    set(openssl_CFLAGS -fvisibility=hidden)
    mindspore_add_pkg(openssl
            VER 1.1.1k
            LIBS ssl crypto
            URL ${REQ_URL}
            SHA256 ${SHA256}
            CONFIGURE_COMMAND ./config no-zlib no-shared
            PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2021-3711.patch
            PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2021-3712.patch
            PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2021-4160.patch
            PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-0778.patch
            PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-1292.patch
            PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-2068.patch
            PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-2097.patch
            PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-4304.patch
            PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-4450.patch
            PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0215.patch
            PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0286.patch
            PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0464.patch
            PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0465.patch
            PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0466.patch
            )
    include_directories(${openssl_INC})
    add_library(mindspore_federated::ssl ALIAS openssl::ssl)
    add_library(mindspore_federated::crypto ALIAS openssl::crypto)
endif()
