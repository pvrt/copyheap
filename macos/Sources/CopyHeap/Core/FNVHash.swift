import Foundation

public enum FNVHash {
    private static let fnvOffsetBasis64: UInt64 = 14695981039346656037
    private static let fnvPrime64: UInt64 = 1099511628211

    public static func fnv1a64(_ text: String) -> UInt64 {
        var hash = fnvOffsetBasis64
        for byte in text.utf8 {
            hash ^= UInt64(byte)
            hash = hash &* fnvPrime64
        }
        return hash
    }
}
