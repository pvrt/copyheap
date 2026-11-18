#include "test_helper.h"
#include "core/storage_manager.h"

using namespace copyheap;

TEST(Limits, Max200UnpinnedEviction) {
    StorageManager sm;
    sm.clear();

    // Add 205 unpinned items
    for (int i = 0; i < 205; ++i) {
        sm.add_or_update_from_clipboard("Item " + std::to_string(i));
    }

    // Must strictly be capped at 200
    ASSERT_EQ(sm.unpinned_count(), 200);

    auto items = sm.get_unpinned_items();
    // Newest is Item 204
    ASSERT_EQ(items.front().content, "Item 204");
    // Oldest surviving should be Item 5 (0, 1, 2, 3, 4 evicted)
    ASSERT_EQ(items.back().content, "Item 5");
}

TEST(Limits, UnlimitedPinnedItems) {
    StorageManager sm;
    sm.clear();

    std::vector<std::string> ids;
    for (int i = 0; i < 50; ++i) {
        std::string id = sm.add_item("Pin " + std::to_string(i), "", false);
        ids.push_back(id);
    }

    ASSERT_EQ(sm.unpinned_count(), 50);
    ASSERT_EQ(sm.pinned_count(), 0);

    // Pin all 50 - unlimited pinning allowed
    for (int i = 0; i < 50; ++i) {
        PinResult res = sm.set_pinned(ids[i], true);
        ASSERT_EQ(static_cast<int>(res), static_cast<int>(PinResult::Success));
    }

    ASSERT_EQ(sm.pinned_count(), 50);
    ASSERT_EQ(sm.unpinned_count(), 0);
}

TEST(Limits, TraySectionsLimit) {
    StorageManager sm;
    sm.clear();

    // Add 150 unpinned
    for (int i = 0; i < 150; ++i) {
        sm.add_or_update_from_clipboard("Unpinned " + std::to_string(i));
    }

    // Add 100 pinned
    for (int i = 0; i < 100; ++i) {
        sm.add_item("Pinned " + std::to_string(i), "", true);
    }

    auto [trayUnpinned, trayPinned] = sm.get_tray_sections();
    // Unpinned recent items in tray must strictly be capped at 30
    ASSERT_EQ(trayUnpinned.size(), 30);
    ASSERT_EQ(trayPinned.size(), 100);
    ASSERT_EQ(trayUnpinned[0].content, "Unpinned 149");
}

TEST(Limits, AtomicJsonSaveAndLoad) {
    StorageManager sm;
    sm.clear();

    sm.add_or_update_from_clipboard("Первый сниппет с кавычками \" и переносом\nвторая строка");
    sm.add_item("Второй сниппет", "Заголовок 2", true);

    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring testFile = std::wstring(tempPath) + L"copyheap_test_history.json";

    ASSERT_TRUE(sm.save_to_file(testFile));

    StorageManager sm2;
    ASSERT_TRUE(sm2.load_from_file(testFile));

    ASSERT_EQ(sm2.unpinned_count(), 1);
    ASSERT_EQ(sm2.pinned_count(), 1);

    auto unpinned = sm2.get_unpinned_items();
    ASSERT_EQ(unpinned[0].content, "Первый сниппет с кавычками \" и переносом\nвторая строка");

    auto pinned = sm2.get_pinned_items();
    ASSERT_EQ(pinned[0].title, "Заголовок 2");
    ASSERT_EQ(pinned[0].content, "Второй сниппет");

    DeleteFileW(testFile.c_str());
}
