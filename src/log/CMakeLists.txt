# =============================================================================
# Quaxis Solo Miner - Log модуль
# =============================================================================
# Терминальный вывод статуса и логирование
# =============================================================================

add_library(quaxis_log STATIC
    status_reporter.cpp
)

target_include_directories(quaxis_log PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(quaxis_log PUBLIC
    quaxis::core
    Threads::Threads
)

add_library(quaxis::log ALIAS quaxis_log)
