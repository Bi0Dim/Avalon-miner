#!/bin/bash
# =============================================================================
# Quaxis Solo Miner - Запуск сервера
# =============================================================================

set -e

# Директория скрипта
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build"
CONFIG_DIR="${HOME}/.config/quaxis"
DEFAULT_CONFIG="${PROJECT_DIR}/config/quaxis.toml.example"

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
  -c, --config FILE    Путь к конфигурационному файлу
  -d, --daemon         Запустить в фоновом режиме
  -v, --verbose        Подробный вывод
  --check-bitcoin      Проверить подключение к Bitcoin Core
  --check-shm          Проверить Shared Memory
  -h, --help           Показать эту справку

Примеры:
  $0                          # Запуск с конфигурацией по умолчанию
  $0 -c /etc/quaxis.toml      # Указать файл конфигурации
  $0 --check-bitcoin          # Проверить подключение к Bitcoin Core
EOF
}

# Проверка подключения к Bitcoin Core
check_bitcoin() {
    log_info "Проверка подключения к Bitcoin Core..."
    
    # Чтение настроек из конфига (простой парсинг)
    if [ -f "$CONFIG_FILE" ]; then
        RPC_USER=$(grep "rpc_user" "$CONFIG_FILE" | cut -d'"' -f2)
        RPC_PASS=$(grep "rpc_password" "$CONFIG_FILE" | cut -d'"' -f2)
        RPC_HOST=$(grep "rpc_host" "$CONFIG_FILE" | cut -d'"' -f2)
        RPC_PORT=$(grep "rpc_port" "$CONFIG_FILE" | grep -oP '\d+')
    else
        RPC_USER="quaxis"
        RPC_PASS="password"
        RPC_HOST="127.0.0.1"
        RPC_PORT="8332"
    fi
    
    RESULT=$(curl -s --user "${RPC_USER}:${RPC_PASS}" \
        --data-binary '{"jsonrpc": "1.0", "method": "getblockchaininfo"}' \
        -H 'content-type: text/plain;' \
        "http://${RPC_HOST}:${RPC_PORT}/" 2>/dev/null || echo "error")
    
    if echo "$RESULT" | grep -q "blocks"; then
        BLOCKS=$(echo "$RESULT" | grep -oP '"blocks":\s*\K\d+')
        log_info "Bitcoin Core подключён! Блоков: $BLOCKS"
        return 0
    else
        log_error "Не удалось подключиться к Bitcoin Core"
        log_error "Проверьте настройки RPC в $CONFIG_FILE"
        return 1
    fi
}

# Проверка Shared Memory
check_shm() {
    log_info "Проверка Shared Memory..."
    
    SHM_PATH="/dev/shm/quaxis_block"
    if [ -f "$SHM_PATH" ]; then
        SIZE=$(stat -c%s "$SHM_PATH")
        log_info "Shared Memory существует: $SHM_PATH (${SIZE} байт)"
        return 0
    else
        log_warn "Shared Memory не найдена: $SHM_PATH"
        log_warn "Убедитесь, что Bitcoin Core запущен с патчами Quaxis"
        return 1
    fi
}

# Установка конфигурации по умолчанию
setup_config() {
    if [ ! -f "${CONFIG_DIR}/quaxis.toml" ]; then
        log_info "Создание конфигурации по умолчанию..."
        mkdir -p "$CONFIG_DIR"
        cp "$DEFAULT_CONFIG" "${CONFIG_DIR}/quaxis.toml"
        log_warn "Конфигурация создана: ${CONFIG_DIR}/quaxis.toml"
        log_warn "ВАЖНО: Отредактируйте файл и укажите свои настройки!"
    fi
}

# Проверка исполняемого файла
check_binary() {
    if [ ! -f "${BUILD_DIR}/quaxis-server" ]; then
        log_error "Исполняемый файл не найден: ${BUILD_DIR}/quaxis-server"
        log_error "Сначала соберите проект: ./scripts/build.sh"
        exit 1
    fi
}

# Запуск сервера
run_server() {
    log_info "Запуск Quaxis Solo Miner Server..."
    
    check_binary
    
    ARGS=""
    
    if [ -n "$CONFIG_FILE" ]; then
        ARGS="$ARGS --config $CONFIG_FILE"
    else
        setup_config
        ARGS="$ARGS --config ${CONFIG_DIR}/quaxis.toml"
    fi
    
    if [ "$VERBOSE" = "1" ]; then
        ARGS="$ARGS --verbose"
    fi
    
    if [ "$DAEMON" = "1" ]; then
        log_info "Запуск в фоновом режиме..."
        nohup "${BUILD_DIR}/quaxis-server" $ARGS > /var/log/quaxis/quaxis.log 2>&1 &
        PID=$!
        echo $PID > /var/run/quaxis.pid
        log_info "Сервер запущен, PID: $PID"
        log_info "Логи: /var/log/quaxis/quaxis.log"
    else
        exec "${BUILD_DIR}/quaxis-server" $ARGS
    fi
}

# Парсинг аргументов
CONFIG_FILE=""
DAEMON=0
VERBOSE=0

while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--config)
            CONFIG_FILE="$2"
            shift 2
            ;;
        -d|--daemon)
            DAEMON=1
            shift
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        --check-bitcoin)
            CONFIG_FILE="${CONFIG_DIR}/quaxis.toml"
            check_bitcoin
            exit $?
            ;;
        --check-shm)
            check_shm
            exit $?
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

# Главная функция
main() {
    log_info "=== Quaxis Solo Miner ==="
    run_server
}

main
