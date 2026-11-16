#include "test_helper.h"
#include "sync/crypto_utils.h"

using namespace copyheap;

TEST(Crypto, Aes256Roundtrip) {
    std::string key = "ghp_secret_token_1234567890";
    std::string plaintext = "Привет, мир! Это секретные сниппеты CopyHeap 12345! @#$%^&*()";

    std::string encrypted = CryptoUtils::encrypt_aes256(plaintext, key);
    ASSERT_TRUE(!encrypted.empty());
    ASSERT_TRUE(encrypted.find("\"encrypted\": true") != std::string::npos);
    ASSERT_TRUE(encrypted.find(plaintext) == std::string::npos); // Plaintext is NOT present

    auto decrypted = CryptoUtils::decrypt_aes256(encrypted, key);
    ASSERT_TRUE(decrypted.has_value());
    ASSERT_EQ(*decrypted, plaintext);
}

TEST(Crypto, WrongKeyFails) {
    std::string key1 = "ghp_correct_token";
    std::string key2 = "ghp_WRONG_token";
    std::string plaintext = "Secret payload";

    std::string encrypted = CryptoUtils::encrypt_aes256(plaintext, key1);
    auto decrypted = CryptoUtils::decrypt_aes256(encrypted, key2);
    // Should fail with wrong key due to padding validation failure
    ASSERT_FALSE(decrypted.has_value());
}

TEST(Crypto, Base64EncodingDecoding) {
    std::string raw = "Arbitrary binary data \x01\x02\x03\x00\xFF\xFE";
    std::string b64 = CryptoUtils::base64_encode(reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
    ASSERT_TRUE(!b64.empty());

    auto decoded = CryptoUtils::base64_decode(b64);
    ASSERT_EQ(decoded.size(), raw.size());
    ASSERT_EQ(std::string(reinterpret_cast<const char*>(decoded.data()), decoded.size()), raw);
}
