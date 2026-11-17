import XCTest
@testable import CopyHeap

final class CopyHeapTests: XCTestCase {
    func testFNV1aHash() {
        let text = "Hello, CopyHeap!"
        let hash1 = FNVHash.fnv1a64(text)
        let hash2 = FNVHash.fnv1a64(text)
        XCTAssertEqual(hash1, hash2)
        XCTAssertNotEqual(hash1, 0)
    }

    func testCryptoAES256EncryptDecrypt() {
        let secret = "My Super Secret Password / GitHub Token 12345"
        let plaintext = "[{\"id\":\"123\",\"content\":\"Test Snippet with Russian text: Привет мир!\",\"is_pinned\":true}]"

        guard let encrypted = CryptoUtils.encryptAES256(plaintext: plaintext, keyMaterial: secret) else {
            XCTFail("Encryption returned nil")
            return
        }

        XCTAssertTrue(encrypted.contains("\"encrypted\": true"))
        XCTAssertTrue(encrypted.contains("\"iv\":"))
        XCTAssertTrue(encrypted.contains("\"ciphertext\":"))

        guard let decrypted = CryptoUtils.decryptAES256(payloadJSON: encrypted, keyMaterial: secret) else {
            XCTFail("Decryption returned nil")
            return
        }

        XCTAssertEqual(decrypted, plaintext)

        // Decrypt with wrong key should fail
        let wrongKeyDecrypted = CryptoUtils.decryptAES256(payloadJSON: encrypted, keyMaterial: "wrong_password")
        XCTAssertNil(wrongKeyDecrypted)
    }

    func testStorageAddAndPrune() {
        let tempDir = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try? FileManager.default.createDirectory(at: tempDir, withIntermediateDirectories: true)
        let storageURL = tempDir.appendingPathComponent("history.json")

        let storage = StorageManager(customURL: storageURL)
        XCTAssertEqual(storage.getAllItems().count, 0)

        // Add 250 items
        for i in 0..<250 {
            storage.addOrUpdate(content: "Item \(i)")
        }

        // Storage limit for unpinned items is 200
        let items = storage.getAllItems()
        XCTAssertEqual(items.count, 200)
        XCTAssertEqual(items.first?.content, "Item 249")

        // Toggle pin
        let firstId = items.first!.id
        let pinRes = storage.togglePin(id: firstId)
        XCTAssertEqual(pinRes, .success)
        XCTAssertEqual(storage.getPinnedItems().count, 1)

        // Search test
        let searchRes = storage.search(query: "Item 249")
        XCTAssertEqual(searchRes.count, 1)
        XCTAssertEqual(searchRes.first?.id, firstId)
    }

    // Test pre-computed lowercase fields
    func testPreComputedLowercase() {
        let item = ClipboardItem(content: "Hello WORLD Привет МИР")
        XCTAssertEqual(item.contentLower, "hello world привет мир")
        XCTAssertFalse(item.contentLower.isEmpty)
    }

    // Test search uses pre-computed lowercase (no crash with Unicode)
    func testSearchUnicode() {
        let tempDir = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try? FileManager.default.createDirectory(at: tempDir, withIntermediateDirectories: true)
        let storage = StorageManager(customURL: tempDir.appendingPathComponent("test.json"))
        storage.addOrUpdate(content: "SELECT * FROM users WHERE id = 42")
        storage.addOrUpdate(content: "Привет мир! Это тест.")
        storage.addOrUpdate(content: "Hello World test")

        // Case-insensitive search in English
        XCTAssertEqual(storage.search(query: "SELECT").count, 1)
        XCTAssertEqual(storage.search(query: "select").count, 1)

        // Case-insensitive search in Russian
        XCTAssertEqual(storage.search(query: "привет").count, 1)
        XCTAssertEqual(storage.search(query: "ПРИВЕТ").count, 1)

        // Empty query returns all
        XCTAssertEqual(storage.search(query: "").count, 3)
    }

    // Test adaptive clipboard polling (verify ClipboardWatcher init doesn't crash)
    func testClipboardWatcherInit() {
        var received = [String]()
        let watcher = ClipboardWatcher { text in received.append(text) }
        watcher.start()
        // Verify timer is running (non-nil after start)
        // Can't easily test timer interval in unit test, just verify no crash
        watcher.stop()
        XCTAssertTrue(received.isEmpty) // no phantom callbacks on start
    }
}
