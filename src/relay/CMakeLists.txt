# =============================================================================
# Quaxis Solo Miner - Модуль UDP Relay
# =============================================================================
# FIBRE-совместимый UDP relay для сверхбыстрого получения блоков
# Латентность: 100-300 мс вместо 500-2000 мс через стандартный P2P
# =============================================================================

add_library(quaxis_relay STATIC
    fec_decoder.cpp
    udp_socket.cpp
    fibre_protocol.cpp
    block_reconstructor.cpp
    relay_peer.cpp
    relay_manager.cpp
)

target_include_directories(quaxis_relay PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(quaxis_relay 
    PUBLIC 
        quaxis_core
        quaxis_crypto
        quaxis_bitcoin
        Threads::Threads
)

# Проверяем наличие Boost.Asio
find_package(Boost QUIET COMPONENTS system)
if(Boost_FOUND)
    target_link_libraries(quaxis_relay PRIVATE Boost::system)
    target_compile_definitions(quaxis_relay PRIVATE QUAXIS_HAS_BOOST_ASIO)
    message(STATUS "UDP Relay: Boost.Asio найден")
endif()

# Алиас для удобства использования
add_library(quaxis::relay ALIAS quaxis_relay)
