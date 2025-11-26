# =============================================================================
# Quaxis Solo Miner - Monitoring модуль
# =============================================================================
# Статистика и логирование
# =============================================================================

add_library(quaxis_monitoring STATIC
    stats.cpp
)

target_include_directories(quaxis_monitoring PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(quaxis_monitoring PUBLIC
    quaxis::core
    quaxis::mining
    quaxis::network
)

add_library(quaxis::monitoring ALIAS quaxis_monitoring)
