#include "test_helper.h"
#include "core/storage_manager.h"
#include "core/string_utils.h"

using namespace copyheap;

TEST(Storage, AddAndRetrieve) {
    StorageManager sm;
    sm.clear();

    ASSERT_TRUE(sm.add_or_update_from_clipboard("Hello World"));
    ASSERT_EQ(sm.unpinned_count(), 1);
    ASSERT_EQ(sm.pinned_count(), 0);

    auto items = sm.get_all_items();
    ASSERT_EQ(items.size(), 1);
    ASSERT_EQ(items[0].content, "Hello World");
    ASSERT_EQ(items[0].get_display_title(), "Hello World");
}

TEST(Storage, DisplayTitleFormatting) {
    ClipboardItem item;
    item.content = "\n\r  \tПервая строка сниппета\r\nВторая строка  ";
    ASSERT_EQ(item.get_display_title(), "Первая строка сниппета");

    item.title = "Пользовательское название";
    ASSERT_EQ(item.get_display_title(), "Пользовательское название");
}

TEST(Storage, DeduplicationUnpinned) {
    StorageManager sm;
    sm.clear();

    sm.add_or_update_from_clipboard("Snippet A");
    sm.add_or_update_from_clipboard("Snippet B");

    auto items = sm.get_all_items();
    ASSERT_EQ(items.size(), 2);
    ASSERT_EQ(items[0].content, "Snippet B");
    ASSERT_EQ(items[1].content, "Snippet A");

    // Give a custom title to Snippet A
    std::string idA = items[1].id;
    sm.update_item(idA, "Custom Title A", "Snippet A");

    // Copying Snippet A again should bring it to the top and preserve custom title
    ASSERT_TRUE(sm.add_or_update_from_clipboard("Snippet A"));
    ASSERT_EQ(sm.unpinned_count(), 2);

    items = sm.get_all_items();
    ASSERT_EQ(items[0].content, "Snippet A");
    ASSERT_EQ(items[0].title, "Custom Title A");
    ASSERT_EQ(items[0].get_display_title(), "Custom Title A");
    ASSERT_EQ(items[1].content, "Snippet B");
}

TEST(Storage, DeduplicationPinnedIgnored) {
    StorageManager sm;
    sm.clear();

    std::string pinnedId = sm.add_item("Important Pinned Snippet", "My Pin", true);
    ASSERT_FALSE(pinnedId.empty());
    ASSERT_EQ(sm.pinned_count(), 1);
    ASSERT_EQ(sm.unpinned_count(), 0);

    // Copying the pinned snippet text should not create a duplicate in unpinned
    bool added = sm.add_or_update_from_clipboard("Important Pinned Snippet");
    // Duplicate not added
    ASSERT_FALSE(added);
    ASSERT_EQ(sm.unpinned_count(), 0);
    ASSERT_EQ(sm.pinned_count(), 1);
}

TEST(Storage, DeleteItem) {
    StorageManager sm;
    sm.clear();

    sm.add_or_update_from_clipboard("To Delete");
    auto items = sm.get_all_items();
    ASSERT_EQ(items.size(), 1);

    ASSERT_TRUE(sm.delete_item(items[0].id));
    ASSERT_EQ(sm.total_count(), 0);
    ASSERT_FALSE(sm.delete_item("non-existent"));
}

TEST(Storage, UpdateItem) {
    StorageManager sm;
    sm.clear();

    sm.add_or_update_from_clipboard("Original Content");
    auto items = sm.get_all_items();
    std::string id = items[0].id;

    ASSERT_TRUE(sm.update_item(id, "New Title", "Updated Content"));
    auto updated = sm.get_item(id);
    ASSERT_TRUE(updated.has_value());
    ASSERT_EQ(updated->title, "New Title");
    ASSERT_EQ(updated->content, "Updated Content");
}

TEST(Storage, ClearHistoryKeepsPinned) {
    StorageManager sm;
    sm.clear();

    sm.add_or_update_from_clipboard("Recent 1");
    sm.add_or_update_from_clipboard("Recent 2");
    sm.add_item("Pinned 1", "Pin Title", true);

    ASSERT_EQ(sm.unpinned_count(), 2);
    ASSERT_EQ(sm.pinned_count(), 1);

    sm.clear_history(true);

    ASSERT_EQ(sm.unpinned_count(), 0);
    ASSERT_EQ(sm.pinned_count(), 1);
    ASSERT_EQ(sm.get_all_items()[0].content, "Pinned 1");

    sm.clear_history(false);
    ASSERT_EQ(sm.total_count(), 0);
}
