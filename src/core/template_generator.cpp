/**
 * @file template_generator.cpp
 * @brief Реализация генератора шаблонов блоков
 */

#include "template_generator.hpp"
#include "primitives/merkle.hpp"
#include "../crypto/sha256.hpp"
#include "../bitcoin/address.hpp"

#include <chrono>
#include <cstring>

namespace quaxis::core {

// =============================================================================
// Константы
// =============================================================================

namespace {

// Версия блока по умолчанию (BIP9)
constexpr int32_t DEFAULT_BLOCK_VERSION = 0x20000000;

// Награда за блок (halving каждые 210000 блоков)
constexpr int64_t BASE_REWARD = 50'00000000LL;  // 50 BTC в satoshi
constexpr uint32_t HALVING_INTERVAL = 210000;

/**
 * @brief Вычислить награду за блок
 */
int64_t calculate_block_reward(uint32_t height) {
    uint32_t halvings = height / HALVING_INTERVAL;
    if (halvings >= 64) {
        return 0;
    }
    return BASE_REWARD >> halvings;
}

} // anonymous namespace

// =============================================================================
// Реализация
// =============================================================================

struct TemplateGenerator::Impl {
    TemplateGeneratorConfig config;
    MtpCalculator mtp_calculator;
    
    // Parsed payout pubkey hash from payout_address
    Hash160 payout_pubkey_hash{};
    bool has_valid_payout_address{false};
    
    // Текущее состояние chain
    Hash256 prev_hash{};
    uint32_t height{0};
    uint32_t bits{0};
    int64_t coinbase_value{0};
    bool has_chain_info{false};
    
    mutable std::mutex mutex_;
    
    explicit Impl(const TemplateGeneratorConfig& cfg) : config(cfg) {
        // Parse the payout address to get the pubkey hash
        if (!config.payout_address.empty()) {
            auto result = bitcoin::parse_p2wpkh_address(config.payout_address);
            if (result) {
                payout_pubkey_hash = *result;
                has_valid_payout_address = true;
            }
        }
    }
    
    /**
     * @brief Создать coinbase транзакцию
     */
    Bytes build_coinbase(uint64_t extranonce) const {
        Bytes coinbase;
        coinbase.reserve(128);
        
        // Version (4 bytes, little-endian)
        coinbase.push_back(0x01);
        coinbase.push_back(0x00);
        coinbase.push_back(0x00);
        coinbase.push_back(0x00);
        
        // Input count (1 byte)
        coinbase.push_back(0x01);
        
        // Previous output hash (32 bytes, all zeros for coinbase)
        coinbase.insert(coinbase.end(), 32, 0x00);
        
        // Previous output index (4 bytes, 0xFFFFFFFF)
        coinbase.push_back(0xFF);
        coinbase.push_back(0xFF);
        coinbase.push_back(0xFF);
        coinbase.push_back(0xFF);
        
        // Build scriptsig
        Bytes scriptsig;
        
        // Block height (BIP34) - serialized as compact size + data
        uint32_t h = height;
        if (h <= 16) {
            scriptsig.push_back(static_cast<uint8_t>(0x50 + h));
        } else if (h <= 0x7F) {
            scriptsig.push_back(0x01);
            scriptsig.push_back(static_cast<uint8_t>(h));
        } else if (h <= 0x7FFF) {
            scriptsig.push_back(0x02);
            scriptsig.push_back(static_cast<uint8_t>(h & 0xFF));
            scriptsig.push_back(static_cast<uint8_t>((h >> 8) & 0xFF));
        } else if (h <= 0x7FFFFF) {
            scriptsig.push_back(0x03);
            scriptsig.push_back(static_cast<uint8_t>(h & 0xFF));
            scriptsig.push_back(static_cast<uint8_t>((h >> 8) & 0xFF));
            scriptsig.push_back(static_cast<uint8_t>((h >> 16) & 0xFF));
        } else {
            scriptsig.push_back(0x04);
            scriptsig.push_back(static_cast<uint8_t>(h & 0xFF));
            scriptsig.push_back(static_cast<uint8_t>((h >> 8) & 0xFF));
            scriptsig.push_back(static_cast<uint8_t>((h >> 16) & 0xFF));
            scriptsig.push_back(static_cast<uint8_t>((h >> 24) & 0xFF));
        }
        
        // Coinbase tag
        for (char c : config.coinbase_tag) {
            scriptsig.push_back(static_cast<uint8_t>(c));
        }
        
        // Extranonce
        for (std::size_t i = 0; i < config.extranonce_size; ++i) {
            scriptsig.push_back(static_cast<uint8_t>((extranonce >> (i * 8)) & 0xFF));
        }
        
        // Scriptsig length
        coinbase.push_back(static_cast<uint8_t>(scriptsig.size()));
        coinbase.insert(coinbase.end(), scriptsig.begin(), scriptsig.end());
        
        // Sequence (4 bytes, 0xFFFFFFFF)
        coinbase.push_back(0xFF);
        coinbase.push_back(0xFF);
        coinbase.push_back(0xFF);
        coinbase.push_back(0xFF);
        
        // Output count (1 byte)
        coinbase.push_back(0x01);
        
        // Output value (8 bytes, little-endian)
        int64_t value = coinbase_value;
        for (int i = 0; i < 8; ++i) {
            coinbase.push_back(static_cast<uint8_t>(value & 0xFF));
            value >>= 8;
        }
        
        // Output script - P2WPKH (22 bytes: OP_0 OP_PUSH20 <20-byte-hash>)
        coinbase.push_back(0x16);  // Script length (22 bytes)
        coinbase.push_back(0x00);  // OP_0
        coinbase.push_back(0x14);  // OP_PUSH20
        
        // Use REAL pubkey hash from parsed payout_address
        // CRITICAL: This ensures mining rewards go to the correct address!
        if (has_valid_payout_address) {
            for (auto byte : payout_pubkey_hash) {
                coinbase.push_back(byte);
            }
        } else {
            // Fallback: 20-byte zero pubkey hash (will trigger validation error on startup)
            // This should never happen in production as validate() checks payout_address
            coinbase.insert(coinbase.end(), 20, 0x00);
        }
        
        // Locktime (4 bytes)
        coinbase.push_back(0x00);
        coinbase.push_back(0x00);
        coinbase.push_back(0x00);
        coinbase.push_back(0x00);
        
        return coinbase;
    }
    
    /**
     * @brief Вычислить midstate первых 64 байт
     */
    std::array<uint8_t, 32> compute_midstate_bytes(const uint8_t* data) const {
        auto state = crypto::compute_midstate(data);
        return crypto::state_to_bytes(state);
    }
};

// =============================================================================
// Публичный API
// =============================================================================

TemplateGenerator::TemplateGenerator(const TemplateGeneratorConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

TemplateGenerator::~TemplateGenerator() = default;

void TemplateGenerator::update_chain_tip(
    const Hash256& prev_hash,
    uint32_t height,
    uint32_t bits,
    int64_t coinbase_value
) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    impl_->prev_hash = prev_hash;
    impl_->height = height;
    impl_->bits = bits;
    impl_->coinbase_value = coinbase_value > 0 ? coinbase_value : calculate_block_reward(height);
    impl_->has_chain_info = true;
}

MtpCalculator& TemplateGenerator::get_mtp_calculator() {
    return impl_->mtp_calculator;
}

std::optional<BlockTemplate> TemplateGenerator::generate_template(uint64_t extranonce) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    if (!impl_->has_chain_info) {
        return std::nullopt;
    }
    
    BlockTemplate tmpl;
    tmpl.height = impl_->height;
    tmpl.bits = impl_->bits;
    tmpl.coinbase_value = impl_->coinbase_value;
    tmpl.is_speculative = false;
    
    // Строим заголовок
    tmpl.header.version = DEFAULT_BLOCK_VERSION;
    tmpl.header.prev_hash = impl_->prev_hash;
    
    // Timestamp
    if (impl_->config.use_mtp_timestamp && impl_->mtp_calculator.has_sufficient_data()) {
        tmpl.header.timestamp = impl_->mtp_calculator.get_min_timestamp();
    } else {
        auto now = std::chrono::system_clock::now();
        tmpl.header.timestamp = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch()
            ).count()
        );
    }
    
    tmpl.header.bits = impl_->bits;
    tmpl.header.nonce = 0;
    
    // Строим coinbase и вычисляем merkle root
    Bytes coinbase = impl_->build_coinbase(extranonce);
    
    // Для пустого блока merkle root = hash(coinbase)
    Hash256 coinbase_hash = crypto::sha256d(ByteSpan(coinbase));
    tmpl.header.merkle_root = coinbase_hash;
    
    // Вычисляем midstates
    if (coinbase.size() >= 64) {
        tmpl.coinbase_midstate = impl_->compute_midstate_bytes(coinbase.data());
    }
    
    // Header midstate
    auto header_bytes = tmpl.header.serialize();
    tmpl.header_midstate = impl_->compute_midstate_bytes(header_bytes.data());
    
    return tmpl;
}

std::optional<BlockTemplate> TemplateGenerator::generate_speculative(
    const Hash256& prev_hash,
    uint64_t extranonce
) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    if (!impl_->has_chain_info) {
        return std::nullopt;
    }
    
    // Генерируем speculative шаблон напрямую
    BlockTemplate tmpl;
    tmpl.height = impl_->height + 1;  // Следующий блок
    tmpl.bits = impl_->bits;
    tmpl.coinbase_value = impl_->coinbase_value;
    tmpl.is_speculative = true;
    
    // Строим заголовок с новым prev_hash
    tmpl.header.version = 0x20000000;  // BIP9
    tmpl.header.prev_hash = prev_hash;
    
    // Timestamp
    if (impl_->config.use_mtp_timestamp && impl_->mtp_calculator.has_sufficient_data()) {
        tmpl.header.timestamp = impl_->mtp_calculator.get_min_timestamp();
    } else {
        auto now = std::chrono::system_clock::now();
        tmpl.header.timestamp = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch()
            ).count()
        );
    }
    
    tmpl.header.bits = impl_->bits;
    tmpl.header.nonce = 0;
    
    // Строим coinbase (используем новую высоту)
    uint32_t old_height = impl_->height;
    impl_->height = tmpl.height;
    Bytes coinbase = impl_->build_coinbase(extranonce);
    impl_->height = old_height;
    
    // Для пустого блока merkle root = hash(coinbase)
    Hash256 coinbase_hash = crypto::sha256d(ByteSpan(coinbase));
    tmpl.header.merkle_root = coinbase_hash;
    
    // Вычисляем midstates
    if (coinbase.size() >= 64) {
        tmpl.coinbase_midstate = impl_->compute_midstate_bytes(coinbase.data());
    }
    
    // Header midstate
    auto header_bytes = tmpl.header.serialize();
    tmpl.header_midstate = impl_->compute_midstate_bytes(header_bytes.data());
    
    return tmpl;
}

bool TemplateGenerator::is_ready() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->has_chain_info;
}

uint32_t TemplateGenerator::current_height() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->height;
}

} // namespace quaxis::core
