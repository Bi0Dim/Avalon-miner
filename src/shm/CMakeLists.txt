# =============================================================================
# Quaxis Solo Miner - Shared Memory Module
# =============================================================================
# Adaptive spin-wait and SHM utilities for ultra-low latency block notifications
# =============================================================================

add_library(quaxis_shm INTERFACE)

target_include_directories(quaxis_shm INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(quaxis::shm ALIAS quaxis_shm)
