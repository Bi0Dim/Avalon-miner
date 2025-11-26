#!/bin/bash
# =============================================================================
# Quaxis Solo Miner - Установка зависимостей
# =============================================================================
# Поддерживаемые ОС: Ubuntu 22.04+, Debian 12+, Fedora 38+, Arch Linux
# =============================================================================

set -e

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Определение дистрибутива
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
        VERSION_ID=$VERSION_ID
    else
        log_error "Не удалось определить дистрибутив"
        exit 1
    fi
}

# Установка для Ubuntu/Debian
install_debian() {
    log_info "Установка зависимостей для Ubuntu/Debian..."
    
    sudo apt update
    
    # Компилятор и инструменты сборки
    sudo apt install -y \
        build-essential \
        cmake \
        ninja-build \
        git \
        pkg-config
    
    # Проверка версии GCC
    GCC_VERSION=$(gcc -dumpversion | cut -d. -f1)
    if [ "$GCC_VERSION" -lt 13 ]; then
        log_warn "GCC версии < 13, устанавливаем GCC 13..."
        sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
        sudo apt update
        sudo apt install -y gcc-13 g++-13
        sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100
        sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100
    fi
    
    # Зависимости
    sudo apt install -y \
        libcurl4-openssl-dev \
        libssl-dev
    
    # Опционально: ARM toolchain для прошивки
    if [ "${INSTALL_ARM_TOOLCHAIN:-0}" = "1" ]; then
        log_info "Установка ARM toolchain..."
        sudo apt install -y gcc-arm-none-eabi
    fi
    
    # Опционально: документация
    if [ "${INSTALL_DOCS:-0}" = "1" ]; then
        log_info "Установка инструментов документации..."
        sudo apt install -y doxygen graphviz
    fi
}

# Установка для Fedora
install_fedora() {
    log_info "Установка зависимостей для Fedora..."
    
    sudo dnf install -y \
        gcc \
        gcc-c++ \
        cmake \
        ninja-build \
        git \
        pkg-config \
        libcurl-devel \
        openssl-devel
    
    # Опционально: ARM toolchain
    if [ "${INSTALL_ARM_TOOLCHAIN:-0}" = "1" ]; then
        log_info "Установка ARM toolchain..."
        sudo dnf install -y arm-none-eabi-gcc arm-none-eabi-newlib
    fi
}

# Установка для Arch Linux
install_arch() {
    log_info "Установка зависимостей для Arch Linux..."
    
    sudo pacman -S --needed --noconfirm \
        base-devel \
        cmake \
        ninja \
        git \
        curl \
        openssl
    
    # Опционально: ARM toolchain
    if [ "${INSTALL_ARM_TOOLCHAIN:-0}" = "1" ]; then
        log_info "Установка ARM toolchain..."
        sudo pacman -S --needed --noconfirm arm-none-eabi-gcc arm-none-eabi-newlib
    fi
}

# Проверка SHA-NI поддержки
check_shani() {
    log_info "Проверка поддержки SHA-NI..."
    if grep -q sha_ni /proc/cpuinfo; then
        log_info "SHA-NI поддерживается! Будут использоваться аппаратные оптимизации."
    else
        log_warn "SHA-NI НЕ поддерживается. Будет использоваться программная реализация."
        log_warn "Для максимальной производительности рекомендуется CPU с SHA-NI:"
        log_warn "  - Intel: Skylake и новее"
        log_warn "  - AMD: Zen и новее"
    fi
}

# Главная функция
main() {
    log_info "=== Установка зависимостей Quaxis Solo Miner ==="
    
    detect_distro
    log_info "Обнаружен дистрибутив: $DISTRO $VERSION_ID"
    
    case "$DISTRO" in
        ubuntu|debian|linuxmint)
            install_debian
            ;;
        fedora|rhel|centos)
            install_fedora
            ;;
        arch|manjaro)
            install_arch
            ;;
        *)
            log_error "Неподдерживаемый дистрибутив: $DISTRO"
            log_error "Установите зависимости вручную (см. docs/BUILDING.md)"
            exit 1
            ;;
    esac
    
    check_shani
    
    log_info "=== Зависимости успешно установлены ==="
    log_info "Для сборки проекта выполните: ./scripts/build.sh"
}

# Обработка аргументов
while [[ $# -gt 0 ]]; do
    case $1 in
        --with-arm)
            INSTALL_ARM_TOOLCHAIN=1
            shift
            ;;
        --with-docs)
            INSTALL_DOCS=1
            shift
            ;;
        -h|--help)
            echo "Использование: $0 [опции]"
            echo ""
            echo "Опции:"
            echo "  --with-arm    Установить ARM toolchain для прошивки ASIC"
            echo "  --with-docs   Установить инструменты документации"
            echo "  -h, --help    Показать эту справку"
            exit 0
            ;;
        *)
            log_error "Неизвестная опция: $1"
            exit 1
            ;;
    esac
done

main
