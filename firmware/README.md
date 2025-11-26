# Avalon 1126 Pro - Кастомная прошивка для Quaxis Solo Miner

## Описание

Минимальная прошивка для ASIC майнера Avalon 1126 Pro, оптимизированная для работы с Quaxis Solo Miner сервером.

## Особенности

- Бинарный протокол 48 байт (вместо Stratum JSON)
- Получение готового midstate от сервера
- Минимальная логика: только SHA256 + compare
- Маленькая очередь заданий (50-100)

## Структура

```
firmware/
├── include/
│   ├── protocol.h    - Определения протокола
│   ├── sha256.h      - SHA256 для контроллера
│   ├── a1126_driver.h - Драйвер чипов A1126
│   ├── network.h     - TCP клиент
│   ├── spi.h         - SPI интерфейс к чипам
│   └── config.h      - Конфигурация прошивки
└── src/
    ├── main.c        - Главный цикл майнинга
    ├── sha256.c      - SHA256 реализация
    ├── a1126_driver.c - Драйвер чипов
    ├── network.c     - Сетевой клиент
    ├── spi.c         - SPI драйвер
    └── utils.c       - Утилиты
```

## Сборка

```bash
make CROSS_COMPILE=arm-linux-gnueabihf-
```

## Формат задания (48 байт)

```
Offset  Size  Field
------  ----  -----
0       32    midstate (SHA256 state после первых 64 байт header)
32      4     timestamp (little-endian)
36      4     bits (little-endian)
40      4     nonce_start (little-endian)
44      4     job_id (little-endian)
```

## Формат ответа (8 байт)

```
Offset  Size  Field
------  ----  -----
0       4     job_id
4       4     nonce (найденный)
```

## Алгоритм работы

1. Получить задание от сервера (48 байт)
2. Загрузить midstate в чипы
3. Перебирать nonce от 0 до 2^32-1
4. Для каждого nonce:
   - Вычислить SHA256(SHA256(midstate || tail))
   - Сравнить с target
   - При совпадении отправить share (8 байт)
5. При получении нового задания - прервать текущее
