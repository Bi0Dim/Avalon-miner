/**
 * @file chain_manager.cpp
 * @brief Реализация менеджера auxiliary chains
 */

#include "chain_manager.hpp"
#include "chains/base_chain.hpp"
#include "chains/fractal_chain.hpp"
#include "chains/rsk_chain.hpp"
#include "chains/syscoin_chain.hpp"
#include "chains/namecoin_chain.hpp"
#include "chains/elastos_chain.hpp"
#include "chains/hathor_chain.hpp"
#include "chains/vcash_chain.hpp"
// Дополнительные chains
#include "chains/myriad_chain.hpp"
#include "chains/huntercoin_chain.hpp"
#include "chains/emercoin_chain.hpp"
#include "chains/unobtanium_chain.hpp"
#include "chains/terracoin_chain.hpp"

#include <thread>
#include <atomic>
#include <condition_variable>

namespace quaxis::merged {

// =============================================================================
// Фабрика chains
// =============================================================================

namespace {

/**
 * @brief Создать chain по имени
 */
std::unique_ptr<IChain> create_chain(const ChainConfig& config) {
    if (config.name == "fractal") {
        return std::make_unique<FractalChain>(config);
    }
    if (config.name == "rsk" || config.name == "rootstock") {
        return std::make_unique<RSKChain>(config);
    }
    if (config.name == "syscoin") {
        return std::make_unique<SyscoinChain>(config);
    }
    if (config.name == "namecoin") {
        return std::make_unique<NamecoinChain>(config);
    }
    if (config.name == "elastos") {
        return std::make_unique<ElastosChain>(config);
    }
    if (config.name == "hathor") {
        return std::make_unique<HathorChain>(config);
    }
    if (config.name == "vcash") {
        return std::make_unique<VCashChain>(config);
    }
    // Дополнительные chains
    if (config.name == "myriad") {
        return std::make_unique<MyriadChain>(config);
    }
    if (config.name == "huntercoin") {
        return std::make_unique<HuntercoinChain>(config);
    }
    if (config.name == "emercoin") {
        return std::make_unique<EmercoinChain>(config);
    }
    if (config.name == "unobtanium") {
        return std::make_unique<UnobtaniumChain>(config);
    }
    if (config.name == "terracoin") {
        return std::make_unique<TerracoinChain>(config);
    }
    
    return nullptr;
}

} // anonymous namespace

// =============================================================================
// ChainManager::Impl
// =============================================================================

struct ChainManager::Impl {
    // Конфигурация
    MergedMiningConfig config;
    
    // Chains
    std::vector<std::unique_ptr<IChain>> chains;
    mutable std::mutex chains_mutex;
    
    // Текущие шаблоны
    std::unordered_map<std::string, AuxBlockTemplate> templates;
    mutable std::mutex templates_mutex;
    
    // Статистика
    std::unordered_map<std::string, uint32_t> block_counts;
    mutable std::mutex stats_mutex;
    
    // Worker thread
    std::thread worker_thread;
    std::atomic<bool> running{false};
    std::condition_variable cv;
    std::mutex cv_mutex;
    
    // Callbacks
    AuxBlockFoundCallback block_found_callback;
    
    explicit Impl(const MergedMiningConfig& cfg) : config(cfg) {
        // Создаём chains из конфигурации
        for (const auto& chain_config : config.chains) {
            if (auto chain = create_chain(chain_config)) {
                chains.push_back(std::move(chain));
            }
        }
    }
    
    ~Impl() {
        stop();
    }
    
    void start() {
        if (running.exchange(true)) {
            return; // Уже запущен
        }
        
        // Подключаемся к chains
        for (auto& chain : chains) {
            if (chain->is_enabled()) {
                [[maybe_unused]] auto result = chain->connect();
            }
        }
        
        // Запускаем worker thread
        worker_thread = std::thread([this] {
            worker_loop();
        });
    }
    
    void stop() {
        if (!running.exchange(false)) {
            return; // Уже остановлен
        }
        
        // Сигнализируем worker thread
        cv.notify_all();
        
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
        
        // Отключаемся от chains
        for (auto& chain : chains) {
            chain->disconnect();
        }
    }
    
    void worker_loop() {
        while (running) {
            update_templates();
            
            // Ждём интервал или сигнал остановки
            std::unique_lock<std::mutex> lock(cv_mutex);
            cv.wait_for(lock, std::chrono::seconds(1), [this] {
                return !running.load();
            });
        }
    }
    
    void update_templates() {
        std::lock_guard<std::mutex> chains_lock(chains_mutex);
        std::lock_guard<std::mutex> templates_lock(templates_mutex);
        
        for (auto& chain : chains) {
            if (!chain->is_enabled() || !chain->is_connected()) {
                continue;
            }
            
            auto result = chain->get_block_template();
            if (result) {
                templates[std::string(chain->name())] = std::move(*result);
            }
        }
    }
    
    std::optional<AuxCommitment> get_aux_commitment() const {
        std::lock_guard<std::mutex> templates_lock(templates_mutex);
        std::lock_guard<std::mutex> chains_lock(chains_mutex);
        
        std::vector<Hash256> aux_hashes;
        std::vector<Hash256> chain_ids;
        
        for (const auto& chain : chains) {
            if (!chain->is_enabled()) {
                continue;
            }
            
            auto it = templates.find(std::string(chain->name()));
            if (it != templates.end()) {
                aux_hashes.push_back(it->second.block_hash);
                chain_ids.push_back(chain->chain_id());
            }
        }
        
        if (aux_hashes.empty()) {
            return std::nullopt;
        }
        
        return create_aux_commitment(aux_hashes, chain_ids);
    }
    
    std::vector<std::string> check_chains(
        const std::array<uint8_t, 80>& parent_header
    ) const {
        std::vector<std::string> matching;
        
        // Вычисляем хеш родительского блока
        Hash256 pow_hash = crypto::sha256d(parent_header);
        
        std::lock_guard<std::mutex> chains_lock(chains_mutex);
        std::lock_guard<std::mutex> templates_lock(templates_mutex);
        
        for (const auto& chain : chains) {
            if (!chain->is_enabled()) {
                continue;
            }
            
            auto it = templates.find(std::string(chain->name()));
            if (it == templates.end()) {
                continue;
            }
            
            if (chain->meets_target(pow_hash, it->second)) {
                matching.push_back(std::string(chain->name()));
            }
        }
        
        return matching;
    }
    
    IChain* find_chain(std::string_view name) {
        for (auto& chain : chains) {
            if (chain->name() == name) {
                return chain.get();
            }
        }
        return nullptr;
    }
    
    const IChain* find_chain(std::string_view name) const {
        for (const auto& chain : chains) {
            if (chain->name() == name) {
                return chain.get();
            }
        }
        return nullptr;
    }
};

// =============================================================================
// ChainManager Implementation
// =============================================================================

ChainManager::ChainManager(const MergedMiningConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

ChainManager::~ChainManager() = default;

void ChainManager::start() {
    impl_->start();
}

void ChainManager::stop() {
    impl_->stop();
}

bool ChainManager::is_running() const noexcept {
    return impl_->running;
}

std::vector<std::string> ChainManager::get_chain_names() const {
    std::lock_guard<std::mutex> lock(impl_->chains_mutex);
    
    std::vector<std::string> names;
    names.reserve(impl_->chains.size());
    
    for (const auto& chain : impl_->chains) {
        names.emplace_back(chain->name());
    }
    
    return names;
}

std::optional<ChainInfo> ChainManager::get_chain_info(
    std::string_view name
) const {
    std::lock_guard<std::mutex> lock(impl_->chains_mutex);
    
    if (const auto* chain = impl_->find_chain(name)) {
        return chain->get_info();
    }
    
    return std::nullopt;
}

std::vector<ChainInfo> ChainManager::get_all_chain_info() const {
    std::lock_guard<std::mutex> lock(impl_->chains_mutex);
    
    std::vector<ChainInfo> result;
    result.reserve(impl_->chains.size());
    
    for (const auto& chain : impl_->chains) {
        result.push_back(chain->get_info());
    }
    
    return result;
}

bool ChainManager::set_chain_enabled(std::string_view name, bool enabled) {
    std::lock_guard<std::mutex> lock(impl_->chains_mutex);
    
    if (auto* chain = impl_->find_chain(name)) {
        chain->set_enabled(enabled);
        
        if (enabled && !chain->is_connected()) {
            [[maybe_unused]] auto result = chain->connect();
        } else if (!enabled && chain->is_connected()) {
            chain->disconnect();
        }
        
        return true;
    }
    
    return false;
}

std::optional<AuxCommitment> ChainManager::get_aux_commitment() const {
    return impl_->get_aux_commitment();
}

std::vector<std::pair<std::string, AuxBlockTemplate>> 
ChainManager::get_active_templates() const {
    std::lock_guard<std::mutex> chains_lock(impl_->chains_mutex);
    std::lock_guard<std::mutex> templates_lock(impl_->templates_mutex);
    
    std::vector<std::pair<std::string, AuxBlockTemplate>> result;
    
    for (const auto& chain : impl_->chains) {
        if (!chain->is_enabled()) {
            continue;
        }
        
        auto it = impl_->templates.find(std::string(chain->name()));
        if (it != impl_->templates.end()) {
            result.emplace_back(std::string(chain->name()), it->second);
        }
    }
    
    return result;
}

std::vector<std::string> ChainManager::check_aux_chains(
    const std::array<uint8_t, 80>& parent_header,
    [[maybe_unused]] const Bytes& coinbase_tx,
    [[maybe_unused]] const MerkleBranch& coinbase_branch
) const {
    return impl_->check_chains(parent_header);
}

Result<void> ChainManager::submit_aux_block(
    std::string_view chain_name,
    const AuxPow& auxpow
) {
    std::lock_guard<std::mutex> chains_lock(impl_->chains_mutex);
    std::lock_guard<std::mutex> templates_lock(impl_->templates_mutex);
    
    auto* chain = impl_->find_chain(chain_name);
    if (!chain) {
        return std::unexpected(Error{ErrorCode::MiningInvalidJob, 
            "Chain не найден: " + std::string(chain_name)});
    }
    
    auto it = impl_->templates.find(std::string(chain_name));
    if (it == impl_->templates.end()) {
        return std::unexpected(Error{ErrorCode::MiningInvalidJob,
            "Нет шаблона для chain: " + std::string(chain_name)});
    }
    
    auto result = chain->submit_block(auxpow, it->second);
    
    if (result) {
        // Обновляем статистику
        std::lock_guard<std::mutex> stats_lock(impl_->stats_mutex);
        impl_->block_counts[std::string(chain_name)]++;
        
        // Вызываем callback
        if (impl_->block_found_callback) {
            impl_->block_found_callback(
                std::string(chain_name),
                it->second.height,
                it->second.block_hash
            );
        }
    }
    
    return result;
}

std::vector<std::pair<std::string, bool>> ChainManager::submit_to_matching_chains(
    const std::array<uint8_t, 80>& parent_header,
    const Bytes& coinbase_tx,
    const MerkleBranch& coinbase_branch
) {
    auto matching = check_aux_chains(parent_header, coinbase_tx, coinbase_branch);
    
    std::vector<std::pair<std::string, bool>> results;
    results.reserve(matching.size());
    
    for (const auto& name : matching) {
        // Создаём AuxPoW
        AuxPow auxpow;
        auxpow.coinbase_tx = coinbase_tx;
        auxpow.coinbase_hash = crypto::sha256d(coinbase_tx);
        auxpow.coinbase_branch = coinbase_branch;
        std::copy(parent_header.begin(), parent_header.end(), 
                  auxpow.parent_header.begin());
        
        // Получаем aux_branch для этой chain
        std::lock_guard<std::mutex> templates_lock(impl_->templates_mutex);
        auto it = impl_->templates.find(name);
        if (it != impl_->templates.end()) {
            // Упрощённо: aux_branch будет пустой для single chain
            // В реальности нужно вычислять branch из aux merkle tree
            auxpow.aux_branch = MerkleBranch{};
        }
        
        auto result = submit_aux_block(name, auxpow);
        results.emplace_back(name, result.has_value());
    }
    
    return results;
}

void ChainManager::set_block_found_callback(AuxBlockFoundCallback callback) {
    impl_->block_found_callback = std::move(callback);
}

std::size_t ChainManager::active_chain_count() const noexcept {
    std::lock_guard<std::mutex> lock(impl_->chains_mutex);
    
    std::size_t count = 0;
    for (const auto& chain : impl_->chains) {
        if (chain->is_enabled() && chain->is_connected()) {
            ++count;
        }
    }
    
    return count;
}

std::unordered_map<std::string, uint32_t> ChainManager::get_block_counts() const {
    std::lock_guard<std::mutex> lock(impl_->stats_mutex);
    return impl_->block_counts;
}

} // namespace quaxis::merged
