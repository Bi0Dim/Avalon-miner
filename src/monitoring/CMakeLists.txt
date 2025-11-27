# =============================================================================
# Quaxis Solo Miner - Monitoring модуль
# =============================================================================
# Статистика, логирование и мониторинг здоровья
# =============================================================================

add_library(quaxis_monitoring STATIC
    stats.cpp
    health_monitor.cpp
    alert_manager.cpp
)

target_include_directories(quaxis_monitoring PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(quaxis_monitoring PUBLIC
    quaxis::core
    quaxis::mining
    quaxis::network
    Threads::Threads
)

add_library(quaxis::monitoring ALIAS quaxis_monitoring)
