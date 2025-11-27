# =============================================================================
# Quaxis Solo Miner - Главный исполняемый файл
# =============================================================================

add_executable(quaxis-miner
    main.cpp
)

target_link_libraries(quaxis-miner PRIVATE
    quaxis::core
    quaxis::crypto
    quaxis::bitcoin
    quaxis::mining
    quaxis::network
    quaxis::log
    quaxis::relay
)

# Установка
install(TARGETS quaxis-miner
    RUNTIME DESTINATION bin
)
