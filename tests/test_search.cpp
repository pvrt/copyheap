#include "test_helper.h"
#include "core/storage_manager.h"

using namespace copyheap;

TEST(Search, EmptyQueryReturnsAll) {
    StorageManager sm;
    sm.clear();

    sm.add_or_update_from_clipboard("One");
    sm.add_or_update_from_clipboard("Two");
    sm.add_item("Three", "Title 3", true);

    auto res = sm.search("");
    ASSERT_EQ(res.size(), 3);
}

TEST(Search, CaseInsensitiveLatinAndCyrillic) {
    StorageManager sm;
    sm.clear();

    sm.add_or_update_from_clipboard("SELECT * FROM users WHERE active = 1;");
    sm.add_or_update_from_clipboard("Привет, Мир! Как дела?");
    sm.add_item("Промпт для нейросети", "НАЗВАНИЕ СНИППЕТА", false);

    // Search lowercase latin
    auto res1 = sm.search("select");
    ASSERT_EQ(res1.size(), 1);
    ASSERT_EQ(res1[0].content, "SELECT * FROM users WHERE active = 1;");

    // Search uppercase latin
    auto res2 = sm.search("USERS");
    ASSERT_EQ(res2.size(), 1);

    // Search lowercase cyrillic
    auto res3 = sm.search("привет");
    ASSERT_EQ(res3.size(), 1);

    // Search uppercase cyrillic matching title
    auto res4 = sm.search("название");
    ASSERT_EQ(res4.size(), 1);
    ASSERT_EQ(res4[0].title, "НАЗВАНИЕ СНИППЕТА");

    // Search partial match in content
    auto res5 = sm.search("нейросети");
    ASSERT_EQ(res5.size(), 1);
}

TEST(Search, OrderingPreserved) {
    StorageManager sm;
    sm.clear();

    sm.add_or_update_from_clipboard("Note 1: Alpha");
    sm.add_or_update_from_clipboard("Note 2: Alpha Beta");
    sm.add_item("Pinned: Alpha", "", true);

    auto res = sm.search("Alpha");
    ASSERT_EQ(res.size(), 3);
    // Unpinned first (newest top)
    ASSERT_EQ(res[0].content, "Note 2: Alpha Beta");
    ASSERT_EQ(res[1].content, "Note 1: Alpha");
    // Pinned after
    ASSERT_EQ(res[2].content, "Pinned: Alpha");
}
