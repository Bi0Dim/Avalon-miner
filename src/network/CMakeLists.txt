# =============================================================================
# Quaxis Solo Miner - Network модуль
# =============================================================================
# TCP сервер и бинарный протокол для ASIC
# =============================================================================

add_library(quaxis_network STATIC
    server.cpp
    asic_connection.cpp
    protocol.cpp
)

target_include_directories(quaxis_network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(quaxis_network PUBLIC
    quaxis::core
    quaxis::mining
    Threads::Threads
)

add_library(quaxis::network ALIAS quaxis_network)
