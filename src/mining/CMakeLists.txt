# =============================================================================
# Quaxis Solo Miner - Mining модуль
# =============================================================================
# Управление заданиями и валидация шар
# =============================================================================

add_library(quaxis_mining STATIC
    job_manager.cpp
    template_cache.cpp
    share_validator.cpp
)

target_include_directories(quaxis_mining PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(quaxis_mining PUBLIC
    quaxis::core
    quaxis::crypto
    quaxis::bitcoin
    Threads::Threads
)

add_library(quaxis::mining ALIAS quaxis_mining)
