# =============================================================================
# Quaxis Solo Miner - HTTP модуль
# =============================================================================
# Простой HTTP сервер для health и metrics endpoints
# =============================================================================

add_library(quaxis_http STATIC
    http_server.cpp
)

target_include_directories(quaxis_http PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(quaxis_http PUBLIC
    quaxis::core
    quaxis::fallback
    Threads::Threads
)

add_library(quaxis::http ALIAS quaxis_http)
