import Foundation
import CommonCrypto

public enum CryptoUtils {
    public static func deriveKeySHA256(input: String) -> Data {
        guard let data = input.data(using: .utf8) else { return Data() }
        var hash = [UInt8](repeating: 0, count: Int(CC_SHA256_DIGEST_LENGTH))
        data.withUnsafeBytes {
            _ = CC_SHA256($0.baseAddress, CC_LONG(data.count), &hash)
        }
        return Data(hash)
    }

    public struct EncryptedPayload: Codable {
        public let encrypted: Bool
        public let iv: String
        public let ciphertext: String
    }

    public static func encryptAES256(plaintext: String, keyMaterial: String) -> String? {
        if keyMaterial.isEmpty { return plaintext }
        guard let plainData = plaintext.data(using: .utf8) else { return nil }

        let keyData = deriveKeySHA256(input: keyMaterial)
        var ivBytes = [UInt8](repeating: 0, count: kCCBlockSizeAES128)
        let status = SecRandomCopyBytes(kSecRandomDefault, ivBytes.count, &ivBytes)
        guard status == errSecSuccess else { return nil }

        let bufferSize = plainData.count + kCCBlockSizeAES128
        var cipherData = Data(count: bufferSize)
        var numBytesEncrypted: size_t = 0

        let cryptStatus = cipherData.withUnsafeMutableBytes { cipherBytes in
            plainData.withUnsafeBytes { dataBytes in
                keyData.withUnsafeBytes { keyBytes in
                    CCCrypt(
                        CCOperation(kCCEncrypt),
                        CCAlgorithm(kCCAlgorithmAES128),
                        CCOptions(kCCOptionPKCS7Padding),
                        keyBytes.baseAddress,
                        kCCKeySizeAES256,
                        ivBytes,
                        dataBytes.baseAddress,
                        plainData.count,
                        cipherBytes.baseAddress,
                        bufferSize,
                        &numBytesEncrypted
                    )
                }
            }
        }

        guard cryptStatus == kCCSuccess else { return nil }
        cipherData.count = numBytesEncrypted

        let b64Iv = Data(ivBytes).base64EncodedString()
        let b64Cipher = cipherData.base64EncodedString()

        return """
        {
          "encrypted": true,
          "iv": "\(b64Iv)",
          "ciphertext": "\(b64Cipher)"
        }
        """
    }

    public static func decryptAES256(payloadJSON: String, keyMaterial: String) -> String? {
        if keyMaterial.isEmpty { return nil }
        guard let data = payloadJSON.data(using: .utf8) else { return nil }

        guard let payload = try? JSONDecoder().decode(EncryptedPayload.self, from: data),
              payload.encrypted,
              let ivData = Data(base64Encoded: payload.iv),
              let cipherData = Data(base64Encoded: payload.ciphertext),
              ivData.count == kCCBlockSizeAES128 else {
            return nil
        }

        let keyData = deriveKeySHA256(input: keyMaterial)
        let bufferSize = cipherData.count + kCCBlockSizeAES128
        var decryptedData = Data(count: bufferSize)
        var numBytesDecrypted: size_t = 0

        let cryptStatus = decryptedData.withUnsafeMutableBytes { decBytes in
            cipherData.withUnsafeBytes { cipherBytes in
                keyData.withUnsafeBytes { keyBytes in
                    ivData.withUnsafeBytes { ivBytes in
                        CCCrypt(
                            CCOperation(kCCDecrypt),
                            CCAlgorithm(kCCAlgorithmAES128),
                            CCOptions(kCCOptionPKCS7Padding),
                            keyBytes.baseAddress,
                            kCCKeySizeAES256,
                            ivBytes.baseAddress,
                            cipherBytes.baseAddress,
                            cipherData.count,
                            decBytes.baseAddress,
                            bufferSize,
                            &numBytesDecrypted
                        )
                    }
                }
            }
        }

        guard cryptStatus == kCCSuccess else { return nil }
        decryptedData.count = numBytesDecrypted
        return String(data: decryptedData, encoding: .utf8)
    }
}
