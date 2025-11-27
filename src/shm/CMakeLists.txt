# =============================================================================
# Quaxis Solo Miner - SHM модуль
# =============================================================================
# Адаптивный spin-wait для Shared Memory
# =============================================================================

add_library(quaxis_shm STATIC
    adaptive_spin.cpp
)

target_include_directories(quaxis_shm PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(quaxis_shm PUBLIC
    quaxis::core
    Threads::Threads
)

add_library(quaxis::shm ALIAS quaxis_shm)
