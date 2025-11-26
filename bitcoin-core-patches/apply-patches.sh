#!/bin/bash
# =============================================================================
# Скрипт применения патчей к Bitcoin Core
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BITCOIN_DIR="${1:-}"

if [ -z "$BITCOIN_DIR" ]; then
    echo "Использование: $0 <путь_к_bitcoin_core>"
    echo ""
    echo "Пример:"
    echo "  $0 ~/bitcoin"
    exit 1
fi

if [ ! -d "$BITCOIN_DIR" ]; then
    echo "Ошибка: директория $BITCOIN_DIR не существует"
    exit 1
fi

if [ ! -f "$BITCOIN_DIR/src/bitcoind.cpp" ]; then
    echo "Ошибка: $BITCOIN_DIR не выглядит как репозиторий Bitcoin Core"
    exit 1
fi

cd "$BITCOIN_DIR"

echo "=== Применение патчей Quaxis к Bitcoin Core ==="
echo "Директория: $BITCOIN_DIR"
echo ""

# Проверяем, что нет незакоммиченных изменений
if ! git diff --quiet; then
    echo "Предупреждение: в репозитории есть незакоммиченные изменения"
    read -p "Продолжить? [y/N] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Применяем патчи по порядку
PATCHES=(
    "0001-quaxis-shm-bridge.patch"
    "0002-spy-mining-callback.patch"
    "0003-block-priority.patch"
)

for patch in "${PATCHES[@]}"; do
    patch_path="$SCRIPT_DIR/$patch"
    
    if [ ! -f "$patch_path" ]; then
        echo "Предупреждение: патч $patch не найден, пропускаем"
        continue
    fi
    
    echo "Применяем $patch..."
    
    if git apply --check "$patch_path" 2>/dev/null; then
        git apply "$patch_path"
        echo "  ✓ Успешно применён"
    else
        echo "  ✗ Ошибка применения патча"
        echo "  Возможно, патч уже применён или конфликтует"
    fi
done

echo ""
echo "=== Готово ==="
echo ""
echo "Следующие шаги:"
echo "1. Соберите Bitcoin Core:"
echo "   ./autogen.sh"
echo "   ./configure --enable-quaxis-shm"
echo "   make -j\$(nproc)"
echo ""
echo "2. Добавьте в bitcoin.conf:"
echo "   quaxisshm=1"
echo "   quaxisshmpath=/quaxis_block"
