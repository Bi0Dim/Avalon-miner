# Bitcoin Core Патчи для Quaxis Solo Miner

## Описание

Эта директория содержит патчи для модификации Bitcoin Core,
обеспечивающие минимальную латентность при получении новых блоков.

## Патчи

1. **0001-quaxis-shm-bridge.patch**
   - Добавляет Shared Memory интерфейс
   - Латентность ~100 нс вместо 1-3 мс через ZMQ
   - Структура QuaxisSharedBlock для передачи данных

2. **0002-spy-mining-callback.patch**
   - Callback в ProcessNewBlockHeaders
   - Уведомление о новых блоках ДО полной валидации
   - Quick PoW check перед уведомлением

3. **0003-block-priority.patch**
   - Приоритет block/headers сообщений в очереди
   - Модификация net_processing.cpp
   - Минимизация задержки обработки

## Применение патчей

```bash
# Клонируем Bitcoin Core
git clone https://github.com/bitcoin/bitcoin.git
cd bitcoin

# Применяем патчи
./apply-patches.sh /path/to/bitcoin

# Или вручную
git apply /path/to/0001-quaxis-shm-bridge.patch
git apply /path/to/0002-spy-mining-callback.patch
git apply /path/to/0003-block-priority.patch

# Собираем Bitcoin Core
./autogen.sh
./configure --enable-quaxis-shm
make -j$(nproc)
```

## Версии Bitcoin Core

Патчи протестированы на:
- Bitcoin Core v27.0
- Bitcoin Core v26.0
- Bitcoin Core v25.0

## Конфигурация

После сборки добавьте в bitcoin.conf:

```ini
# Включить Quaxis Shared Memory
quaxisshm=1
quaxisshmpath=/quaxis_block
```
