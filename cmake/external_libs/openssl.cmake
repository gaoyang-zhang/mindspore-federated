if(ENABLE_GITEE)
    set(REQ_URL "https://gitee.com/mirrors/openssl/repository/archive/OpenSSL_1_1_1k.tar.gz")
    set(MD5 "bdd51a68ad74618dd2519da8e0bcc759")
else()
    set(REQ_URL "https://github.com/openssl/openssl/archive/refs/tags/OpenSSL_1_1_1k.tar.gz")
    set(MD5 "bdd51a68ad74618dd2519da8e0bcc759")
endif()
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    mindspore_add_pkg(openssl
            VER 1.1.0
            LIBS ssl crypto
            URL ${REQ_URL}
            MD5 ${MD5}
            CONFIGURE_COMMAND ./config no-zlib no-shared)
    include_directories(${openssl_INC})
    add_library(mindspore_federated::ssl ALIAS openssl::ssl)
    add_library(mindspore_federated::crypto ALIAS openssl::crypto)
endif()
