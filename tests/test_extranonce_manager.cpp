/**
 * @file test_extranonce_manager.cpp
 * @brief Tests for ExtrannonceManager
 * 
 * Validates that per-connection extranonce management works correctly
 * to prevent duplicate work across multiple ASIC connections.
 */

#include <gtest/gtest.h>
#include <algorithm>
#include "mining/extranonce_manager.hpp"

namespace quaxis::tests {

class ExtrannonceManagerTest : public ::testing::Test {
protected:
    mining::ExtrannonceManager manager_{1};  // Start from 1
};

/**
 * @brief Test: assign unique extranonces to connections
 */
TEST_F(ExtrannonceManagerTest, AssignUniqueExtranonces) {
    auto ext1 = manager_.assign_extranonce(100);
    auto ext2 = manager_.assign_extranonce(200);
    auto ext3 = manager_.assign_extranonce(300);
    
    // Each should be unique
    EXPECT_NE(ext1, ext2);
    EXPECT_NE(ext2, ext3);
    EXPECT_NE(ext1, ext3);
    
    // Should be sequential starting from 1
    EXPECT_EQ(ext1, 1);
    EXPECT_EQ(ext2, 2);
    EXPECT_EQ(ext3, 3);
}

/**
 * @brief Test: retrieve extranonce by connection ID
 */
TEST_F(ExtrannonceManagerTest, GetExtranonce) {
    auto assigned = manager_.assign_extranonce(42);
    
    auto retrieved = manager_.get_extranonce(42);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(*retrieved, assigned);
    
    // Non-existent connection returns nullopt
    auto missing = manager_.get_extranonce(999);
    EXPECT_FALSE(missing.has_value());
}

/**
 * @brief Test: release extranonce on disconnect
 */
TEST_F(ExtrannonceManagerTest, ReleaseExtranonce) {
    [[maybe_unused]] auto ext = manager_.assign_extranonce(100);
    
    EXPECT_TRUE(manager_.has_extranonce(100));
    EXPECT_EQ(manager_.active_count(), 1);
    
    manager_.release_extranonce(100);
    
    EXPECT_FALSE(manager_.has_extranonce(100));
    EXPECT_EQ(manager_.active_count(), 0);
}

/**
 * @brief Test: extranonces are not reused after release
 */
TEST_F(ExtrannonceManagerTest, NoExtrannonceReuse) {
    auto ext1 = manager_.assign_extranonce(100);
    manager_.release_extranonce(100);
    
    // Assign to new connection - should NOT reuse ext1
    auto ext2 = manager_.assign_extranonce(200);
    EXPECT_GT(ext2, ext1);
}

/**
 * @brief Test: multiple connections tracking
 */
TEST_F(ExtrannonceManagerTest, MultipleConnections) {
    [[maybe_unused]] auto ext1 = manager_.assign_extranonce(1);
    [[maybe_unused]] auto ext2 = manager_.assign_extranonce(2);
    [[maybe_unused]] auto ext3 = manager_.assign_extranonce(3);
    
    EXPECT_EQ(manager_.active_count(), 3);
    
    auto connections = manager_.get_active_connections();
    EXPECT_EQ(connections.size(), 3);
    
    // All connection IDs should be present
    EXPECT_TRUE(std::find(connections.begin(), connections.end(), 1) != connections.end());
    EXPECT_TRUE(std::find(connections.begin(), connections.end(), 2) != connections.end());
    EXPECT_TRUE(std::find(connections.begin(), connections.end(), 3) != connections.end());
}

/**
 * @brief Test: peek next extranonce
 */
TEST_F(ExtrannonceManagerTest, PeekNextExtranonce) {
    EXPECT_EQ(manager_.peek_next_extranonce(), 1);
    
    [[maybe_unused]] auto ext1 = manager_.assign_extranonce(100);
    EXPECT_EQ(manager_.peek_next_extranonce(), 2);
    
    [[maybe_unused]] auto ext2 = manager_.assign_extranonce(200);
    EXPECT_EQ(manager_.peek_next_extranonce(), 3);
}

/**
 * @brief Test: custom start value
 */
TEST(ExtrannonceManagerStartValueTest, CustomStartValue) {
    mining::ExtrannonceManager manager(1000);
    
    auto ext = manager.assign_extranonce(1);
    EXPECT_EQ(ext, 1000);
}

/**
 * @brief Test: concurrent access simulation
 * 
 * While this test runs sequentially, it validates the operations
 * work correctly in rapid succession, as would happen with multiple connections.
 */
TEST_F(ExtrannonceManagerTest, RapidAssignRelease) {
    // Simulate rapid connect/disconnect cycles
    for (int i = 0; i < 100; ++i) {
        uint32_t conn_id = static_cast<uint32_t>(i);
        auto ext = manager_.assign_extranonce(conn_id);
        
        // Verify assignment
        EXPECT_TRUE(manager_.has_extranonce(conn_id));
        EXPECT_EQ(*manager_.get_extranonce(conn_id), ext);
        
        // Release half of them
        if (i % 2 == 0) {
            manager_.release_extranonce(conn_id);
            EXPECT_FALSE(manager_.has_extranonce(conn_id));
        }
    }
    
    // Should have 50 active connections (odd numbered)
    EXPECT_EQ(manager_.active_count(), 50);
}

} // namespace quaxis::tests
