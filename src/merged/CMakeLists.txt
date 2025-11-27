# =============================================================================
# Quaxis Solo Miner - Merged Mining модуль
# =============================================================================
# Поддержка merged mining (AuxPoW) для одновременного майнинга
# Bitcoin и дополнительных SHA-256 монет
# =============================================================================

add_library(quaxis_merged STATIC
    auxpow.cpp
    chain_manager.cpp
    merged_job_creator.cpp
    reward_dispatcher.cpp
    chains/base_chain.cpp
    chains/fractal_chain.cpp
    chains/rsk_chain.cpp
    chains/syscoin_chain.cpp
    chains/namecoin_chain.cpp
    chains/elastos_chain.cpp
    chains/hathor_chain.cpp
    chains/vcash_chain.cpp
    # Дополнительные chains
    chains/stacks_chain.cpp
    chains/myriad_chain.cpp
    chains/huntercoin_chain.cpp
    chains/emercoin_chain.cpp
    chains/unobtanium_chain.cpp
    chains/terracoin_chain.cpp
    rpc/aux_rpc_client.cpp
)

target_include_directories(quaxis_merged PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(quaxis_merged PUBLIC
    quaxis::core
    quaxis::crypto
    quaxis::bitcoin
    CURL::libcurl
    Threads::Threads
)

add_library(quaxis::merged ALIAS quaxis_merged)
