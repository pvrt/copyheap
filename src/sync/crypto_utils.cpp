#include "crypto_utils.h"

#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <sstream>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

namespace copyheap {

std::string CryptoUtils::base64_encode(const uint8_t* data, size_t len) {
    if (!data || len == 0) return "";
    DWORD b64Len = 0;
    if (!CryptBinaryToStringA(data, static_cast<DWORD>(len), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &b64Len)) {
        return "";
    }
    std::string result(b64Len, '\0');
    if (!CryptBinaryToStringA(data, static_cast<DWORD>(len), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &result[0], &b64Len)) {
        return "";
    }
    while (!result.empty() && (result.back() == '\0' || result.back() == '\r' || result.back() == '\n')) {
        result.pop_back();
    }
    return result;
}

std::vector<uint8_t> CryptoUtils::base64_decode(const std::string& b64) {
    if (b64.empty()) return {};
    DWORD outLen = 0;
    if (!CryptStringToBinaryA(b64.c_str(), static_cast<DWORD>(b64.length()), CRYPT_STRING_BASE64, nullptr, &outLen, nullptr, nullptr)) {
        return {};
    }
    std::vector<uint8_t> result(outLen);
    if (!CryptStringToBinaryA(b64.c_str(), static_cast<DWORD>(b64.length()), CRYPT_STRING_BASE64, result.data(), &outLen, nullptr, nullptr)) {
        return {};
    }
    result.resize(outLen);
    return result;
}

std::vector<uint8_t> CryptoUtils::derive_key_sha256(const std::string& input) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    std::vector<uint8_t> hash(32, 0);

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0) {
        BCRYPT_HASH_HANDLE hHash = nullptr;
        if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) >= 0) {
            BCryptHashData(hHash, (PUCHAR)input.data(), static_cast<ULONG>(input.length()), 0);
            BCryptFinishHash(hHash, hash.data(), static_cast<ULONG>(hash.size()), 0);
            BCryptDestroyHash(hHash);
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }
    return hash;
}

std::string CryptoUtils::encrypt_aes256(const std::string& plaintext, const std::string& key_material) {
    if (key_material.empty()) return plaintext;

    std::vector<uint8_t> keyBytes = derive_key_sha256(key_material);

    // Generate 16-byte random IV
    std::vector<uint8_t> iv(16);
    if (BCryptGenRandom(nullptr, iv.data(), static_cast<ULONG>(iv.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
        return "";
    }

    // Copy IV as BCryptEncrypt mutates it
    std::vector<uint8_t> ivCopy = iv;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) < 0) {
        return "";
    }

    BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);

    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, keyBytes.data(), static_cast<ULONG>(keyBytes.size()), 0) < 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    }

    ULONG cipherLen = 0;
    // Determine buffer size with PKCS#7 padding
    BCryptEncrypt(hKey, (PUCHAR)plaintext.data(), static_cast<ULONG>(plaintext.length()),
                  nullptr, ivCopy.data(), static_cast<ULONG>(ivCopy.size()),
                  nullptr, 0, &cipherLen, BCRYPT_BLOCK_PADDING);

    std::vector<uint8_t> cipher(cipherLen);
    ivCopy = iv; // reset IV

    ULONG bytesWritten = 0;
    NTSTATUS status = BCryptEncrypt(hKey, (PUCHAR)plaintext.data(), static_cast<ULONG>(plaintext.length()),
                                    nullptr, ivCopy.data(), static_cast<ULONG>(ivCopy.size()),
                                    cipher.data(), static_cast<ULONG>(cipher.size()),
                                    &bytesWritten, BCRYPT_BLOCK_PADDING);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (status < 0) {
        return "";
    }

    cipher.resize(bytesWritten);

    std::string b64Iv = base64_encode(iv.data(), iv.size());
    std::string b64Cipher = base64_encode(cipher.data(), cipher.size());

    std::ostringstream ss;
    ss << "{\n"
       << "  \"encrypted\": true,\n"
       << "  \"iv\": \"" << b64Iv << "\",\n"
       << "  \"ciphertext\": \"" << b64Cipher << "\"\n"
       << "}\n";

    return ss.str();
}

std::optional<std::string> CryptoUtils::decrypt_aes256(const std::string& payload_json, const std::string& key_material) {
    if (key_material.empty()) return std::nullopt;

    auto extract_field = [&](const std::string& key) -> std::string {
        std::string pattern = "\"" + key + "\":";
        size_t pos = payload_json.find(pattern);
        if (pos == std::string::npos) return "";
        pos = payload_json.find('"', pos + pattern.size());
        if (pos == std::string::npos) return "";
        size_t end = payload_json.find('"', pos + 1);
        if (end == std::string::npos) return "";
        return payload_json.substr(pos + 1, end - pos - 1);
    };

    if (payload_json.find("\"encrypted\":") == std::string::npos ||
        payload_json.find("true") == std::string::npos) {
        return std::nullopt;
    }

    std::string b64Iv = extract_field("iv");
    std::string b64Cipher = extract_field("ciphertext");

    if (b64Iv.empty() || b64Cipher.empty()) {
        return std::nullopt;
    }

    std::vector<uint8_t> iv = base64_decode(b64Iv);
    std::vector<uint8_t> cipher = base64_decode(b64Cipher);

    if (iv.size() != 16 || cipher.empty()) {
        return std::nullopt;
    }

    std::vector<uint8_t> keyBytes = derive_key_sha256(key_material);

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) < 0) {
        return std::nullopt;
    }

    BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);

    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, keyBytes.data(), static_cast<ULONG>(keyBytes.size()), 0) < 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::nullopt;
    }

    std::vector<uint8_t> ivCopy = iv;
    ULONG plainLen = 0;

    BCryptDecrypt(hKey, cipher.data(), static_cast<ULONG>(cipher.size()),
                  nullptr, ivCopy.data(), static_cast<ULONG>(ivCopy.size()),
                  nullptr, 0, &plainLen, BCRYPT_BLOCK_PADDING);

    std::vector<uint8_t> plain(plainLen);
    ivCopy = iv;

    ULONG bytesWritten = 0;
    NTSTATUS status = BCryptDecrypt(hKey, cipher.data(), static_cast<ULONG>(cipher.size()),
                                    nullptr, ivCopy.data(), static_cast<ULONG>(ivCopy.size()),
                                    plain.data(), static_cast<ULONG>(plain.size()),
                                    &bytesWritten, BCRYPT_BLOCK_PADDING);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (status < 0) {
        return std::nullopt; // Bad key or corrupted padding
    }

    plain.resize(bytesWritten);
    return std::string(reinterpret_cast<const char*>(plain.data()), plain.size());
}

} // namespace copyheap
