# Оптимизации Quaxis Solo Miner

## Обзор оптимизаций

Quaxis использует 17 оптимизаций для максимальной эффективности соло-майнинга.

## Категория 1: Вычислительные оптимизации

### 1. Готовый midstate от сервера (+3.3% эффективности)

**Суть**: Сервер вычисляет SHA256 midstate для первых 64 байт header.
ASIC получает готовый midstate и делает только 1 SHA256 transform вместо 2.

**Как работает**:
```
Стандартный подход:
SHA256(SHA256(header[0:80])) = 2 × SHA256 = 128 байт → 2 transform

С midstate:
midstate = SHA256_partial(header[0:64])  // Сервер
SHA256(midstate || header[64:80]) = 1 transform  // ASIC

Экономия: 50% вычислений SHA256 = +3.3% эффективности
```

**Задание для ASIC**:
```
midstate[32] + header_tail[12] + job_id[4] = 48 байт
```

### 2. Пустой блок (только coinbase) (+0.5%)

**Суть**: Не включаем транзакции из mempool, блок содержит только coinbase.

**Преимущества**:
- Merkle root = SHA256d(coinbase_hash) - одна операция
- Блок ~190 байт вместо 1-4 MB
- Быстрее распространение в сети
- Меньше времени на сборку блока

**Когда использовать**:
- Всегда для соло-майнинга
- Транзакции собирает другой майнер после нахождения блока

### 3. SHA-NI ускорение на сервере

**Суть**: Использование аппаратных SHA256 инструкций Intel/AMD.

**Intrinsics**:
```cpp
_mm_sha256rnds2_epu32()   // SHA256 round
_mm_sha256msg1_epu32()    // Message schedule 1
_mm_sha256msg2_epu32()    // Message schedule 2
```

**Производительность**:
- SHA-NI: ~100 наносекунд на SHA256
- Generic: ~500 наносекунд на SHA256
- Ускорение: 5x

**Проверка поддержки**:
```cpp
#include <cpuid.h>
bool has_shani = __builtin_cpu_supports("sha");
```

### 4. Предвычисление coinbase midstate

**Суть**: Первые 64 байта coinbase не меняются при смене extranonce.
Вычисляем midstate один раз при новом блоке.

**Структура coinbase**:
```
Часть 1 (64 байта) - КОНСТАНТА:
[version][input_count][prev_tx][index][scriptsig_len][height]["quaxis"][padding]

Часть 2 (46 байт) - ПЕРЕМЕННАЯ:
[extranonce][sequence][outputs][locktime]
```

**Оптимизация**:
```cpp
// При новом блоке - один раз
coinbase_midstate = sha256_partial(coinbase_part1);

// При смене extranonce - быстро
coinbase_hash = sha256_finalize(coinbase_midstate, coinbase_part2);
```

## Категория 2: Уникальность хешей

### 5. Coinbase tag "quaxis"

**Суть**: 6-байтная метка `71 75 61 78 69 73` ("quaxis") в scriptsig coinbase.

**Зачем**:
- Уникальная идентификация блоков
- Нет конфликта с другими майнерами
- Удобство мониторинга в блокчейне

### 6. MTP+1 timestamp

**Суть**: Используем минимально допустимый timestamp (Median Time Past + 1).

**Правила Bitcoin**:
- timestamp >= MTP (медиана последних 11 блоков)
- timestamp <= current_time + 2 hours

**Преимущество**: При равном хешрейте блок с меньшим timestamp имеет чуть
больше шансов (при одновременном нахождении ноды предпочтут меньший timestamp).

### 7. Уникальный payout address

**Суть**: Использование P2WPKH (Native SegWit) адреса.

**Формат**: bc1q... (bech32)

**Преимущества**:
- Меньше размер транзакции
- Меньше комиссия при трате
- Уникальная идентификация

### 8. Extranonce 6 байт

**Суть**: Пространство 2^48 = 281 триллион вариантов.

**Расчёт**:
```
Hashrate: 90 TH/s = 90 × 10^12 H/s
Nonce: 2^32 = 4.3 × 10^9 вариантов
Extranonce: 2^48 = 2.8 × 10^14 вариантов

Время исчерпания nonce: 4.3 × 10^9 / 90 × 10^12 = 47 мс
Время исчерпания всех вариантов: 2.8 × 10^14 × 47 мс = 589 лет
```

## Категория 3: Минимизация латентности

### 9. Shared Memory вместо ZMQ (~100 нс вместо 1-3 мс)

**Суть**: POSIX shared memory для уведомлений о новых блоках.

**Реализация**:
```cpp
// Bitcoin Core (запись)
int fd = shm_open("/quaxis_block", O_RDWR | O_CREAT, 0666);
QuaxisSharedBlock* shm = mmap(...);
std::memcpy(shm->header_raw, block.header, 80);
shm->sequence.fetch_add(1, std::memory_order_release);

// Quaxis Server (чтение, spin-wait)
while (shm->sequence.load(std::memory_order_acquire) == last_seq) {
    _mm_pause();  // Снижение потребления CPU
}
```

**Структура QuaxisSharedBlock**:
```cpp
struct QuaxisSharedBlock {
    alignas(64) std::atomic<uint64_t> sequence;  // Cache-line выровнен
    alignas(64) std::atomic<uint8_t> state;      // 0=empty, 1=speculative, 2=confirmed
    alignas(64) uint8_t header_raw[80];
    uint32_t height;
    uint32_t bits;
    uint32_t timestamp;
    int64_t coinbase_value;
    uint8_t block_hash[32];
};
```

### 10. Spy Mining (экономия 150-1500 мс)

**Суть**: Майнинг начинается сразу после получения header, НЕ ждём полной 
валидации блока.

**Как работает**:
```
1. Получен новый header от сети
2. Quick PoW check (header.hash <= target) - 1 мкс
3. Уведомление Quaxis Server (state=SPECULATIVE)
4. Начало майнинга на height+1
5. Параллельная валидация блока в Bitcoin Core
6. При невалидном блоке - откат (state=INVALID)
```

**Риски**:
- Вероятность невалидного блока: ~0.001%
- При откате: потеря ~1 секунды работы
- Ожидаемый выигрыш: 150-1500 мс × 99.999% > 0

### 11. Callback в ProcessNewBlockHeaders

**Суть**: Модификация Bitcoin Core для мгновенного уведомления.

**Патч**:
```cpp
// validation.cpp
void ChainstateManager::ProcessNewBlockHeaders(...) {
    // Quick PoW check
    if (CheckProofOfWork(header.GetHash(), header.nBits)) {
        // Уведомление через Shared Memory
        g_quaxis_shm->WriteHeader(header, QUAXIS_STATE_SPECULATIVE);
    }
    // ... стандартная валидация ...
}
```

### 12. Приоритет block messages

**Суть**: Block/Headers сообщения обрабатываются первыми в очереди.

**Патч net_processing.cpp**:
```cpp
void PeerManager::ProcessMessages(...) {
    // Сначала блоки и заголовки
    ProcessBlockRelatedMessages();
    // Потом всё остальное
    ProcessOtherMessages();
}
```

### 13. Предвычисление шаблонов

**Суть**: Готовим шаблон блока N+1 пока майним блок N.

**Что предвычисляем**:
- height = current_height + 1
- prev_block_hash = current_best_hash
- coinbase с правильным reward
- Midstate coinbase

**При новом блоке**:
```cpp
void OnNewBlock(const Block& block) {
    if (block.hash == precomputed_template.prev_block_hash) {
        // Шаблон готов! Только обновить prev_block_hash
        current_template = precomputed_template;
        current_template.prev_block_hash = block.hash;
    } else {
        // Реорг или пропущенный блок
        BuildNewTemplate();
    }
    StartPrecomputingNextTemplate();
}
```

## Категория 4: Протокол связи с ASIC

### 14. Бинарный протокол 48 байт

**Сравнение**:
| Метрика | Stratum JSON | Quaxis Binary |
|---------|--------------|---------------|
| Размер задания | 500-1000 байт | 48 байт |
| Размер ответа | 200-300 байт | 8 байт |
| Парсинг | JSON (медленно) | memcpy (быстро) |
| Накладные расходы | Высокие | Минимальные |

### 15. Минимальная логика на ASIC

**Что делает ASIC**:
```c
void mining_loop() {
    while (1) {
        Job job = receive_job();      // 48 байт
        load_midstate(job.midstate);  // Загрузка в чипы
        
        for (nonce = 0; nonce < 0xFFFFFFFF; nonce++) {
            hash = sha256_final(job.header_tail, nonce);
            if (hash <= target) {
                send_share(job.job_id, nonce);  // 8 байт
            }
            if (new_job_available()) break;
        }
    }
}
```

### 16. Маленькая очередь заданий

**Суть**: Только 50-100 заданий в очереди для минимизации stale work.

**Расчёт**:
```
Время на задание: 47 мс (при 90 TH/s)
Очередь 100: 4.7 секунды работы
При новом блоке: макс. 4.7 секунды stale work
```

## Категория 5: Сетевые оптимизации

### 17. Оптимизированный bitcoin.conf

**Ключевые настройки**:
```ini
# Больше соединений = быстрее получение блоков
maxconnections=256
maxoutboundconnections=16

# Compact Blocks для быстрого распространения
blockreconstructionextratxn=100000

# Большой mempool для compact blocks
maxmempool=1000

# Все ядра для валидации
par=0

# Большой кеш для быстрого доступа
dbcache=4096

# Отключение wallet (не нужен для майнинга)
disablewallet=1
```

## Суммарный эффект

| Оптимизация | Выигрыш |
|-------------|---------|
| Midstate | +3.3% эффективности |
| Пустые блоки | +0.5% |
| SHA-NI | -400 нс на операцию |
| SHM | -1-3 мс латентности |
| Spy Mining | -150-1500 мс на блок |
| Бинарный протокол | 10x меньше данных |

**Итого**: ~4% прирост эффективности + значительное снижение латентности.
