#!/bin/bash
# =============================================================================
# Quaxis Solo Miner - Настройка Bitcoin Core с патчами
# =============================================================================

set -e

# Директория скрипта
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PATCHES_DIR="${PROJECT_DIR}/bitcoin-core-patches"

# Параметры
BITCOIN_VERSION="v26.0"
BITCOIN_DIR=""
INSTALL_PREFIX="/usr/local"

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
Использование: $0 [опции] <путь к bitcoin>

Опции:
  --version VERSION    Версия Bitcoin Core (по умолчанию: v26.0)
  --prefix PATH        Путь установки (по умолчанию: /usr/local)
  --clone              Клонировать Bitcoin Core в указанную директорию
  --apply-only         Только применить патчи (без сборки)
  -h, --help           Показать эту справку

Примеры:
  $0 --clone /opt/bitcoin          # Клонировать и собрать
  $0 /path/to/bitcoin              # Применить патчи к существующему
  $0 --apply-only /path/to/bitcoin # Только патчи
EOF
}

# Клонирование Bitcoin Core
clone_bitcoin() {
    log_info "Клонирование Bitcoin Core $BITCOIN_VERSION..."
    
    if [ -d "$BITCOIN_DIR" ]; then
        log_error "Директория уже существует: $BITCOIN_DIR"
        exit 1
    fi
    
    git clone https://github.com/bitcoin/bitcoin.git "$BITCOIN_DIR"
    cd "$BITCOIN_DIR"
    git checkout "$BITCOIN_VERSION"
    
    log_info "Bitcoin Core клонирован в $BITCOIN_DIR"
}

# Применение патчей
apply_patches() {
    log_info "Применение патчей Quaxis..."
    
    cd "$BITCOIN_DIR"
    
    # Проверка, что мы в правильной директории
    if [ ! -f "configure.ac" ]; then
        log_error "Не найден configure.ac - это не директория Bitcoin Core"
        exit 1
    fi
    
    # Применение патчей по порядку
    for patch in "$PATCHES_DIR"/0*.patch; do
        PATCH_NAME=$(basename "$patch")
        log_info "Применение $PATCH_NAME..."
        
        if git apply --check "$patch" 2>/dev/null; then
            git apply "$patch"
            log_info "  ✓ Применён успешно"
        else
            log_warn "  Патч уже применён или конфликт: $PATCH_NAME"
        fi
    done
    
    log_info "Все патчи применены"
}

# Сборка Bitcoin Core
build_bitcoin() {
    log_info "Сборка Bitcoin Core..."
    
    cd "$BITCOIN_DIR"
    
    # Установка зависимостей (для Ubuntu/Debian)
    if command -v apt &> /dev/null; then
        log_info "Установка зависимостей сборки..."
        sudo apt install -y \
            build-essential \
            libtool \
            autotools-dev \
            automake \
            pkg-config \
            bsdmainutils \
            python3 \
            libssl-dev \
            libevent-dev \
            libboost-system-dev \
            libboost-filesystem-dev \
            libboost-chrono-dev \
            libboost-test-dev \
            libboost-thread-dev \
            libzmq3-dev
    fi
    
    # Генерация configure
    if [ ! -f "configure" ]; then
        log_info "Генерация configure..."
        ./autogen.sh
    fi
    
    # Конфигурация
    log_info "Конфигурация..."
    ./configure \
        --prefix="$INSTALL_PREFIX" \
        --without-gui \
        --with-zmq \
        --enable-reduce-exports \
        --disable-bench \
        --disable-tests
    
    # Сборка
    log_info "Компиляция (это может занять 10-30 минут)..."
    make -j$(nproc)
    
    log_info "Сборка завершена"
}

# Установка
install_bitcoin() {
    log_info "Установка Bitcoin Core..."
    
    cd "$BITCOIN_DIR"
    sudo make install
    
    log_info "Bitcoin Core установлен в $INSTALL_PREFIX"
    
    # Проверка
    if "$INSTALL_PREFIX/bin/bitcoind" --version | grep -q "quaxis"; then
        log_info "✓ Патчи Quaxis активны"
    else
        log_warn "Патчи могут быть неактивны, проверьте версию"
    fi
}

# Настройка конфигурации
setup_config() {
    log_info "Настройка конфигурации Bitcoin Core..."
    
    BITCOIN_CONF_DIR="$HOME/.bitcoin"
    mkdir -p "$BITCOIN_CONF_DIR"
    
    if [ -f "$BITCOIN_CONF_DIR/bitcoin.conf" ]; then
        log_warn "bitcoin.conf уже существует, создаю резервную копию..."
        cp "$BITCOIN_CONF_DIR/bitcoin.conf" "$BITCOIN_CONF_DIR/bitcoin.conf.bak"
    fi
    
    cp "$PROJECT_DIR/config/bitcoin.conf.optimized" "$BITCOIN_CONF_DIR/bitcoin.conf"
    
    log_warn "ВАЖНО: Отредактируйте $BITCOIN_CONF_DIR/bitcoin.conf"
    log_warn "Установите rpcuser и rpcpassword!"
}

# Парсинг аргументов
CLONE=0
APPLY_ONLY=0

while [[ $# -gt 0 ]]; do
    case $1 in
        --version)
            BITCOIN_VERSION="$2"
            shift 2
            ;;
        --prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        --clone)
            CLONE=1
            shift
            ;;
        --apply-only)
            APPLY_ONLY=1
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            if [ -z "$BITCOIN_DIR" ]; then
                BITCOIN_DIR="$1"
                shift
            else
                log_error "Неизвестная опция: $1"
                show_help
                exit 1
            fi
            ;;
    esac
done

# Проверка аргументов
if [ -z "$BITCOIN_DIR" ]; then
    log_error "Укажите путь к Bitcoin Core"
    show_help
    exit 1
fi

# Главная функция
main() {
    log_info "=== Настройка Bitcoin Core для Quaxis ==="
    log_info "Версия: $BITCOIN_VERSION"
    log_info "Директория: $BITCOIN_DIR"
    
    if [ "$CLONE" = "1" ]; then
        clone_bitcoin
    fi
    
    apply_patches
    
    if [ "$APPLY_ONLY" = "0" ]; then
        build_bitcoin
        install_bitcoin
        setup_config
    fi
    
    log_info "=== Настройка завершена ==="
    log_info ""
    log_info "Следующие шаги:"
    log_info "1. Отредактируйте ~/.bitcoin/bitcoin.conf"
    log_info "2. Запустите: bitcoind -daemon"
    log_info "3. Дождитесь синхронизации: bitcoin-cli getblockchaininfo"
    log_info "4. Запустите Quaxis: ./scripts/run-server.sh"
}

main
