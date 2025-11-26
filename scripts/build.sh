#!/bin/bash
# =============================================================================
# Quaxis Solo Miner - Скрипт сборки
# =============================================================================

set -e

# Директория скрипта
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build"

# Параметры по умолчанию
BUILD_TYPE="Release"
GENERATOR="Ninja"
ENABLE_TESTS="ON"
ENABLE_SHANI="ON"
JOBS=$(nproc)

# Цвета
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Помощь
show_help() {
    cat << EOF
Использование: $0 [опции]

Опции:
  --debug           Сборка в режиме отладки
  --release         Сборка в режиме release (по умолчанию)
  --no-tests        Не собирать тесты
  --no-shani        Отключить SHA-NI оптимизации
  --clean           Очистить директорию сборки
  --jobs N          Количество параллельных задач (по умолчанию: все ядра)
  -h, --help        Показать эту справку

Примеры:
  $0                    # Release сборка со всеми опциями
  $0 --debug            # Debug сборка с sanitizers
  $0 --clean --release  # Чистая release сборка
EOF
}

# Парсинг аргументов
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --debug)
                BUILD_TYPE="Debug"
                shift
                ;;
            --release)
                BUILD_TYPE="Release"
                shift
                ;;
            --no-tests)
                ENABLE_TESTS="OFF"
                shift
                ;;
            --no-shani)
                ENABLE_SHANI="OFF"
                shift
                ;;
            --clean)
                CLEAN_BUILD=1
                shift
                ;;
            --jobs)
                JOBS="$2"
                shift 2
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                log_error "Неизвестная опция: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

# Проверка зависимостей
check_dependencies() {
    log_info "Проверка зависимостей..."
    
    # CMake
    if ! command -v cmake &> /dev/null; then
        log_error "CMake не найден. Установите: sudo apt install cmake"
        exit 1
    fi
    
    CMAKE_VERSION=$(cmake --version | head -1 | awk '{print $3}')
    CMAKE_MAJOR=$(echo "$CMAKE_VERSION" | cut -d. -f1)
    CMAKE_MINOR=$(echo "$CMAKE_VERSION" | cut -d. -f2)
    if [ "$CMAKE_MAJOR" -lt 3 ] || ([ "$CMAKE_MAJOR" -eq 3 ] && [ "$CMAKE_MINOR" -lt 25 ]); then
        log_error "Требуется CMake 3.25+, установлен $CMAKE_VERSION"
        exit 1
    fi
    
    # Ninja (опционально, но рекомендуется)
    if ! command -v ninja &> /dev/null; then
        log_warn "Ninja не найден, используется Make"
        GENERATOR="Unix Makefiles"
    fi
    
    # Компилятор
    if command -v g++ &> /dev/null; then
        GCC_VERSION=$(g++ -dumpversion | cut -d. -f1)
        if [ "$GCC_VERSION" -lt 13 ]; then
            log_warn "GCC версии $GCC_VERSION, рекомендуется 13+"
        fi
    elif command -v clang++ &> /dev/null; then
        CLANG_VERSION=$(clang++ --version | head -1 | grep -oP '\d+' | head -1)
        if [ "$CLANG_VERSION" -lt 17 ]; then
            log_warn "Clang версии $CLANG_VERSION, рекомендуется 17+"
        fi
    else
        log_error "C++ компилятор не найден"
        exit 1
    fi
    
    log_info "Все зависимости в порядке"
}

# Очистка сборки
clean_build() {
    if [ -d "$BUILD_DIR" ]; then
        log_info "Очистка директории сборки..."
        rm -rf "$BUILD_DIR"
    fi
}

# Конфигурация CMake
configure_cmake() {
    log_info "Конфигурация CMake..."
    log_info "  Тип сборки: $BUILD_TYPE"
    log_info "  Генератор: $GENERATOR"
    log_info "  Тесты: $ENABLE_TESTS"
    log_info "  SHA-NI: $ENABLE_SHANI"
    
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    cmake -G "$GENERATOR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DQUAXIS_ENABLE_TESTS="$ENABLE_TESTS" \
        -DQUAXIS_ENABLE_SHANI="$ENABLE_SHANI" \
        "$PROJECT_DIR"
}

# Сборка
build_project() {
    log_info "Сборка проекта (jobs=$JOBS)..."
    cd "$BUILD_DIR"
    
    if [ "$GENERATOR" = "Ninja" ]; then
        ninja -j "$JOBS"
    else
        make -j "$JOBS"
    fi
}

# Запуск тестов
run_tests() {
    if [ "$ENABLE_TESTS" = "ON" ]; then
        log_info "Запуск тестов..."
        cd "$BUILD_DIR"
        ctest --output-on-failure
    fi
}

# Главная функция
main() {
    parse_args "$@"
    
    log_info "=== Сборка Quaxis Solo Miner ==="
    
    check_dependencies
    
    if [ "${CLEAN_BUILD:-0}" = "1" ]; then
        clean_build
    fi
    
    configure_cmake
    build_project
    
    if [ "$BUILD_TYPE" = "Release" ]; then
        run_tests
    fi
    
    log_info "=== Сборка завершена успешно ==="
    log_info "Исполняемый файл: $BUILD_DIR/quaxis-server"
    
    if [ "$ENABLE_TESTS" = "ON" ]; then
        log_info "Тесты: $BUILD_DIR/tests/quaxis_tests"
    fi
}

main "$@"
