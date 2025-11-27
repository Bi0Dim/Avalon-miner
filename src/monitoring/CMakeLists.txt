# =============================================================================
# Quaxis Solo Miner - Monitoring модуль
# =============================================================================
# Статистика, логирование, мониторинг здоровья и метрики Prometheus
# =============================================================================

add_library(quaxis_monitoring STATIC
    stats.cpp
    health_monitor.cpp
    alert_manager.cpp
    metrics.cpp
    alerter.cpp
)

target_include_directories(quaxis_monitoring PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(quaxis_monitoring PUBLIC
    quaxis::core
    quaxis::mining
    quaxis::network
    Threads::Threads
)

add_library(quaxis::monitoring ALIAS quaxis_monitoring)
