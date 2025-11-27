# =============================================================================
# Quaxis Solo Miner - Тесты
# =============================================================================

# Создание исполняемого файла тестов
add_executable(quaxis_tests
    test_sha256.cpp
    test_coinbase.cpp
    test_block.cpp
    test_protocol.cpp
    test_target.cpp
    test_relay.cpp
)

# Добавление include директории для тестов
target_include_directories(quaxis_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Подключение зависимостей
target_link_libraries(quaxis_tests PRIVATE
    quaxis_core
    quaxis_crypto
    quaxis_bitcoin
    quaxis_mining
    quaxis_network
    quaxis_relay
    GTest::gtest
    GTest::gtest_main
)

# Добавление тестов в CTest
include(GoogleTest)
gtest_discover_tests(quaxis_tests)

# Бенчмарк SHM vs ZMQ
if(QUAXIS_ENABLE_BENCHMARKS)
    add_executable(benchmark_shm_vs_zmq
        benchmark_shm_vs_zmq.cpp
    )
    
    target_include_directories(benchmark_shm_vs_zmq PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    
    target_link_libraries(benchmark_shm_vs_zmq PRIVATE
        quaxis_core
        quaxis_bitcoin
        Threads::Threads
    )
endif()
