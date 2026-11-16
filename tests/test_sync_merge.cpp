#include "test_helper.h"
#include "core/storage_manager.h"
#include "core/models.h"

#include <filesystem>

using namespace copyheap;

TEST(SyncMerge, MergePinnedItems) {
    std::wstring testPath = L"test_sync_merge_unit.json";
    if (std::filesystem::exists(testPath)) {
        std::filesystem::remove(testPath);
    }

    StorageManager storage;
    storage.clear();
    storage.add_item("Local Pinned 1", "L1", true);
    storage.add_item("Local Pinned 2", "L2", true);

    ASSERT_EQ(storage.pinned_count(), 2);

    // Prepare remote items: 1 update, 1 new
    auto local_items = storage.get_pinned_items();
    std::vector<ClipboardItem> remote;

    // Updated item with newer timestamp
    ClipboardItem updated = local_items[0];
    updated.title = "L1 Updated from Cloud";
    updated.pinned_at += 10000;
    remote.push_back(updated);

    // New item from remote with newest pinned_at
    ClipboardItem remote_new;
    remote_new.id = "remote_id_999";
    remote_new.content = "Remote New Item";
    remote_new.title = "R_New";
    remote_new.is_pinned = true;
    remote_new.created_at = 1000;
    remote_new.pinned_at = local_items[0].pinned_at + 50000;
    remote.push_back(remote_new);

    bool changed = storage.merge_pinned(remote);
    ASSERT_TRUE(changed);
    ASSERT_EQ(storage.pinned_count(), 3);

    auto merged = storage.get_pinned_items();
    // Most recent pinned item should be first
    ASSERT_EQ(merged[0].id, "remote_id_999");
    ASSERT_EQ(merged[0].title, "R_New");

    // Serialization & deserialization check
    std::string json_str = storage.serialize_pinned_json();
    auto roundtrip = StorageManager::deserialize_pinned_json(json_str);
    ASSERT_EQ(roundtrip.size(), 3);
    ASSERT_EQ(roundtrip[0].title, "R_New");

    storage.flush_save();
    std::filesystem::remove(testPath);
}
