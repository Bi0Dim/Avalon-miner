# =============================================================================
# Quaxis Solo Miner - Bitcoin модуль
# =============================================================================
# Работа с блоками, транзакциями и RPC
# =============================================================================

add_library(quaxis_bitcoin STATIC
    block.cpp
    coinbase.cpp
    target.cpp
    address.cpp
    rpc_client.cpp
    shm_subscriber.cpp
)

target_include_directories(quaxis_bitcoin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(quaxis_bitcoin PUBLIC
    quaxis::core
    quaxis::crypto
    CURL::libcurl
    Threads::Threads
)

add_library(quaxis::bitcoin ALIAS quaxis_bitcoin)
