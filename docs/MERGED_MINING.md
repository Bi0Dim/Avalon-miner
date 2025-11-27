# Merged Mining (AuxPoW) — Дополнительный доход

## Обзор

Merged Mining позволяет майнить Bitcoin и 6+ дополнительных SHA-256 криптовалют **одновременно**, используя один и тот же Proof of Work. Это означает, что вы можете получать награды от нескольких блокчейнов **без потери хешрейта**.

## Принцип работы

### AuxPoW (Auxiliary Proof of Work)

1. **Bitcoin** является родительской цепочкой (parent chain)
2. **Auxiliary chains** используют PoW Bitcoin в качестве доказательства работы
3. При каждой итерации майнинга Bitcoin, хеш проверяется против target'ов всех auxiliary chains
4. Если хеш меньше target какой-либо aux chain — блок отправляется в эту chain

### Merkle Tree для AuxPoW

```
         AuxPoW Merkle Root
              /        \
         H(0,1)        H(2,3)
        /     \       /     \
   Chain0  Chain1  Chain2  Chain3
```

Commitment помещается в coinbase транзакцию Bitcoin:
```
AUXPOW_MAGIC (4 bytes) || AuxMerkleRoot (32 bytes) || TreeSize (4 bytes) || Nonce (4 bytes)
```

## Поддерживаемые монеты

| Монета | Тикер | Ожидаемый доход/мес* | RPC порт по умолчанию |
|--------|-------|---------------------|------------------------|
| Fractal Bitcoin | FB | $25-41 | 8332 |
| RSK/Rootstock | RBTC | $10-20 | 4444 |
| Syscoin | SYS | $8-12 | 8370 |
| Namecoin | NMC | $8-10 | 8336 |
| Elastos | ELA | $3-5 | 20336 |
| Hathor | HTR | $5-15 | 8080 |
| VCash | XVC | <$1 | 5739 |
| Stacks | STX | Experimental* | 20443 |
| Myriad | XMY | <$1 | 10888 |
| Huntercoin | HUC | <$1 | 8398 |
| Emercoin | EMC | $1-3 | 6662 |
| Unobtanium | UNO | <$1 | 65530 |
| Terracoin | TRC | <$1 | 13332 |

\* При хешрейте 90 TH/s. Доходность зависит от курса и сложности.
\* Stacks использует Proof of Transfer (PoX), экспериментальная поддержка.

**Суммарный дополнительный доход: $65-115/месяц (13 монет)**

## Конфигурация

### Основные настройки

В файле `quaxis.toml`:

```toml
[merged_mining]
enabled = true
health_check_interval = 60  # секунды
```

### Настройка отдельных chains

```toml
[[merged_mining.chains]]
name = "fractal"           # Имя chain
enabled = true             # Включена ли chain
rpc_url = "http://127.0.0.1:8332"  # URL ноды
rpc_user = "quaxis"        # RPC пользователь
rpc_password = "password"  # RPC пароль
priority = 100            # Приоритет (выше = важнее)
rpc_timeout = 30          # Таймаут RPC (секунды)
update_interval = 5       # Интервал обновления шаблона
```

### Приоритеты

Приоритет используется для:
1. Разрешения коллизий slot ID в Merkle tree
2. Определения порядка отправки блоков

Рекомендуемые приоритеты (по доходности):
- Fractal Bitcoin: 100
- RSK: 90
- Syscoin: 80
- Namecoin: 70
- Elastos: 60
- Hathor: 50
- VCash: 10

## Установка нод

### Fractal Bitcoin

```bash
# Скачать и запустить Fractal Bitcoin
git clone https://github.com/fractal-bitcoin/fractal-bitcoin.git
cd fractal-bitcoin
./configure && make
./fractald -daemon

# bitcoin.conf для Fractal
server=1
rpcuser=quaxis
rpcpassword=your_password
rpcport=8332
rpcallowip=127.0.0.1
```

### Namecoin

```bash
# Установка через apt (Ubuntu/Debian)
sudo apt install namecoin-daemon

# namecoin.conf
server=1
rpcuser=quaxis
rpcpassword=your_password
auxpow=1  # Важно: включить AuxPoW!
```

### RSK (Rootstock)

RSK использует другой RPC протокол. Смотрите [документацию RSK](https://developers.rsk.co/).

```bash
# Запуск RSKj
java -jar rskj-core-*.jar --regtest
```

### Syscoin

```bash
# syscoin.conf
server=1
rpcuser=quaxis
rpcpassword=your_password
auxpow=1
```

## Архитектура модуля

```
src/merged/
├── CMakeLists.txt           # Сборка модуля
├── auxpow.hpp/cpp           # AuxPoW структуры, Merkle tree
├── chain_interface.hpp      # Интерфейс для chains
├── chain_manager.hpp/cpp    # Менеджер всех chains
├── merged_job_creator.hpp/cpp # Создание заданий с AuxPoW
├── reward_dispatcher.hpp/cpp  # Отправка блоков
├── chains/
│   ├── base_chain.hpp/cpp   # Базовый класс chain
│   ├── fractal_chain.hpp/cpp
│   ├── rsk_chain.hpp/cpp
│   ├── syscoin_chain.hpp/cpp
│   ├── namecoin_chain.hpp/cpp
│   ├── elastos_chain.hpp/cpp
│   ├── hathor_chain.hpp/cpp
│   ├── vcash_chain.hpp/cpp
│   ├── stacks_chain.hpp/cpp     # Stacks (STX) — Experimental PoX
│   ├── myriad_chain.hpp/cpp     # Myriad (XMY) — Multi-algo
│   ├── huntercoin_chain.hpp/cpp # Huntercoin (HUC)
│   ├── emercoin_chain.hpp/cpp   # Emercoin (EMC)
│   ├── unobtanium_chain.hpp/cpp # Unobtanium (UNO)
│   └── terracoin_chain.hpp/cpp  # Terracoin (TRC)
└── rpc/
    └── aux_rpc_client.hpp/cpp  # RPC клиент для aux chains
```

## Использование API

### Получение AuxPoW commitment

```cpp
#include "merged/chain_manager.hpp"

// Создаём менеджер
MergedMiningConfig config;
config.enabled = true;
// ... настраиваем chains

ChainManager manager(config);
manager.start();

// Получаем commitment для coinbase
auto commitment = manager.get_aux_commitment();
if (commitment) {
    // Добавляем в coinbase
    auto data = commitment->serialize();
    // ...
}
```

### Проверка и отправка блоков

```cpp
#include "merged/reward_dispatcher.hpp"

RewardDispatcher dispatcher(chain_manager);

// При нахождении блока Bitcoin
auto results = dispatcher.dispatch_block(
    header,
    coinbase_tx,
    nonce,
    merged_job
);

for (const auto& result : results) {
    if (result.success) {
        std::cout << "Блок отправлен в " << result.chain_name 
                  << " на высоте " << result.height << std::endl;
    }
}
```

## Мониторинг

### Статистика chains

```cpp
auto infos = chain_manager.get_all_chain_info();
for (const auto& info : infos) {
    std::cout << info.name << ": " 
              << to_string(info.status)
              << ", height=" << info.height
              << ", difficulty=" << info.difficulty
              << std::endl;
}
```

### Количество найденных блоков

```cpp
auto counts = chain_manager.get_block_counts();
for (const auto& [name, count] : counts) {
    std::cout << name << ": " << count << " блоков" << std::endl;
}
```

## Безопасность

### RPC авторизация

Всегда используйте сложные пароли для RPC:

```toml
rpc_password = "$(openssl rand -hex 32)"
```

### Firewall

Разрешите RPC только с localhost:

```bash
iptables -A INPUT -p tcp --dport 8332 -s 127.0.0.1 -j ACCEPT
iptables -A INPUT -p tcp --dport 8332 -j DROP
```

## Устранение неполадок

### Chain не подключается

1. Проверьте, запущена ли нода: `./chain-cli getblockchaininfo`
2. Проверьте RPC credentials
3. Проверьте, включен ли AuxPoW в конфигурации ноды

### Блоки не принимаются

1. Убедитесь, что нода синхронизирована
2. Проверьте логи ноды на наличие ошибок
3. Убедитесь, что используется правильный chain ID

### Низкая доходность

1. Проверьте, все ли chains активны
2. Обновите ноды до последних версий
3. Проверьте сетевые задержки

## Дополнительные монеты

### Stacks (STX)

**EXPERIMENTAL**: Stacks использует Proof of Transfer (PoX) вместо классического AuxPoW.

```bash
# Установка Stacks ноды
# См. https://docs.stacks.co/docs/nodes-and-miners/
docker run -d stacks/stacks-blockchain

# API порт по умолчанию: 20443
```

**Примечание**: Требуется дополнительное исследование совместимости.

### Myriad (XMY)

Multi-algo монета с 5 алгоритмами. Только SHA256 поддерживает merged mining.

```bash
# myriad.conf
server=1
rpcuser=quaxis
rpcpassword=your_password
auxpow=1
rpcport=10888
```

### Huntercoin (HUC)

Игровая монета с классическим AuxPoW.

```bash
# huntercoin.conf
server=1
rpcuser=quaxis
rpcpassword=your_password
auxpow=1
rpcport=8398
```

### Emercoin (EMC)

Блокчейн платформа для DNS, SSL, SSH.

```bash
# emercoin.conf
server=1
rpcuser=quaxis
rpcpassword=your_password
auxpow=1
rpcport=6662
```

### Unobtanium (UNO)

Редкая монета с максимальным supply 250,000 UNO.

```bash
# unobtanium.conf
server=1
rpcuser=quaxis
rpcpassword=your_password
auxpow=1
rpcport=65530
```

### Terracoin (TRC)

Старый форк Bitcoin с поддержкой merged mining.

```bash
# terracoin.conf
server=1
rpcuser=quaxis
rpcpassword=your_password
auxpow=1
rpcport=13332
```

## FAQ

**Q: Теряется ли хешрейт при merged mining?**
A: Нет. Используется тот же PoW, что и для Bitcoin.

**Q: Можно ли добавить свою chain?**
A: Да. Создайте класс, наследующий `BaseChain`, и добавьте в `ChainManager`.

**Q: Что если aux chain недоступна?**
A: Майнинг Bitcoin продолжается. Недоступные chains автоматически отключаются.

**Q: Как часто обновляются шаблоны aux chains?**
A: По умолчанию каждые 5 секунд. Настраивается через `update_interval`.

## Ссылки

- [BIP 0034](https://github.com/bitcoin/bips/blob/master/bip-0034.mediawiki) - Block v2, Height in Coinbase
- [Namecoin AuxPoW](https://www.namecoin.org/docs/auxpow/) - Документация Namecoin
- [RSK Merged Mining](https://developers.rsk.co/rsk/architecture/mining/) - RSK документация
