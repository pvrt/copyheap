// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "CopyHeap",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        .executable(name: "CopyHeap", targets: ["CopyHeap"])
    ],
    dependencies: [],
    targets: [
        .executableTarget(
            name: "CopyHeap",
            dependencies: [],
            path: "Sources/CopyHeap"
        ),
        .testTarget(
            name: "CopyHeapTests",
            dependencies: ["CopyHeap"],
            path: "Tests/CopyHeapTests"
        )
    ]
)
