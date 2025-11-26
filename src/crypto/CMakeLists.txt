# =============================================================================
# Quaxis Solo Miner - Crypto модуль
# =============================================================================
# SHA256 с поддержкой SHA-NI (Intel/AMD)
# =============================================================================

add_library(quaxis_crypto STATIC
    sha256.cpp
    sha256_generic.cpp
)

# Добавляем SHA-NI реализацию если поддерживается
if(COMPILER_SUPPORTS_SHA)
    target_sources(quaxis_crypto PRIVATE sha256_shani.cpp)
    set_source_files_properties(sha256_shani.cpp PROPERTIES
        COMPILE_FLAGS "-msha -msse4.1"
    )
endif()

target_include_directories(quaxis_crypto PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(quaxis_crypto PUBLIC
    quaxis::core
)

add_library(quaxis::crypto ALIAS quaxis_crypto)
