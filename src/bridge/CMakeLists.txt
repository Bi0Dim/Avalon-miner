# =============================================================================
# Quaxis Solo Miner - Bridge модуль
# =============================================================================
# Мост к Bitcoin Core с поддержкой fallback
# =============================================================================

add_library(quaxis_bridge STATIC
    bitcoin_bridge.cpp
)

target_include_directories(quaxis_bridge PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(quaxis_bridge PUBLIC
    quaxis::core
    quaxis::bitcoin
    quaxis::fallback
    quaxis::monitoring
    Threads::Threads
)

add_library(quaxis::bridge ALIAS quaxis_bridge)
