# =============================================================================
# Quaxis Solo Miner - Core Sync модуль
# =============================================================================
# Синхронизация заголовков
# =============================================================================

add_library(quaxis_sync STATIC
    headers_store.cpp
    headers_sync.cpp
)

target_include_directories(quaxis_sync PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(quaxis_sync PUBLIC
    quaxis::core
    quaxis::primitives
    quaxis::validation
)

# Алиас для удобства использования
add_library(quaxis::sync ALIAS quaxis_sync)
