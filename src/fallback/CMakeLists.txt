# =============================================================================
# Quaxis Solo Miner - Fallback модуль
# =============================================================================
# Автоматическое переключение между источниками заданий
# =============================================================================

add_library(quaxis_fallback STATIC
    pool_config.cpp
    stratum_client.cpp
    fallback_manager.cpp
)

target_include_directories(quaxis_fallback PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(quaxis_fallback PUBLIC
    quaxis::core
    Threads::Threads
)

add_library(quaxis::fallback ALIAS quaxis_fallback)
