# =============================================================================
# Quaxis Solo Miner - SHM модуль
# =============================================================================
# Адаптивное ожидание изменений в shared memory
# =============================================================================

add_library(quaxis_shm STATIC
    adaptive_spin.cpp
)

add_library(quaxis::shm ALIAS quaxis_shm)

target_include_directories(quaxis_shm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(quaxis_shm PUBLIC
    Threads::Threads
)
