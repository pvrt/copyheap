#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace copyheap {

class CryptoUtils {
public:
    // Derives a 32-byte (256-bit) AES key from a passphrase/token via SHA-256
    static std::vector<uint8_t> derive_key_sha256(const std::string& input);

    // Encrypts plaintext using AES-256-CBC with PKCS#7 padding and a random 16-byte IV.
    // Returns a JSON string: {"encrypted":true,"iv":"<base64>","ciphertext":"<base64>"}
    static std::string encrypt_aes256(const std::string& plaintext, const std::string& key_material);

    // Decrypts payload produced by encrypt_aes256. Returns decrypted plaintext,
    // or std::nullopt if key is invalid, data corrupted or not encrypted.
    static std::optional<std::string> decrypt_aes256(const std::string& payload_json, const std::string& key_material);

    // Base64 helpers using CryptBinaryToStringA / CryptStringToBinaryA
    static std::string base64_encode(const uint8_t* data, size_t len);
    static std::vector<uint8_t> base64_decode(const std::string& b64);
};

} // namespace copyheap
