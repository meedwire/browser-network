#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

PROJECT_NAME="browser_network_core"
OUTPUT_DIR="$REPO_ROOT/integrations/react-native-network/ios/Frameworks"
BUILD_DIR="$REPO_ROOT/build/ios_core"

# Detect CPU cores
NPROC=$(sysctl -n hw.ncpu || nproc || echo 4)
echo "Using $NPROC CPU cores for build"

mkdir -p "$OUTPUT_DIR"
mkdir -p "$BUILD_DIR"

create_combined_archive() {
    PLATFORM=$1
    CURRENT_BUILD_DIR="$BUILD_DIR/$PLATFORM"
    OUTPUT_ARCHIVE="$CURRENT_BUILD_DIR/lib${PROJECT_NAME}_combined.a"
    LIBS=(
        "$CURRENT_BUILD_DIR/libbrowser_network_core.a"
        "$CURRENT_BUILD_DIR/_deps/curl-build/lib/libcurl.a"
        "$CURRENT_BUILD_DIR/_deps/ixwebsocket-build/libixwebsocket.a"
    )

    while IFS= read -r lib; do
        LIBS+=("$lib")
    done < <(find "$CURRENT_BUILD_DIR/_deps/abseil-cpp-build" -type f -name '*.a' | sort)

    if [ -d "$CURRENT_BUILD_DIR/_deps/mbedtls-build/library" ]; then
        while IFS= read -r lib; do
            LIBS+=("$lib")
        done < <(find "$CURRENT_BUILD_DIR/_deps/mbedtls-build/library" -type f -name '*.a' | sort)
    fi

    /usr/bin/libtool -static -o "$OUTPUT_ARCHIVE" "${LIBS[@]}"
}

build_for_platform() {
    PLATFORM=$1 # "iOS" or "Simulator"
    ARCH=$2     # "arm64" or "arm64;x86_64"
    SDK=$3      # "iphoneos" or "iphonesimulator"
    
    echo "----------------------------------------"
    echo "Building for $PLATFORM ($ARCH)..."
    echo "----------------------------------------"
    
    CURRENT_BUILD_DIR="$BUILD_DIR/$PLATFORM"
    mkdir -p "$CURRENT_BUILD_DIR"
    
    cmake \
        -H"$REPO_ROOT/cpp" \
        -B"$CURRENT_BUILD_DIR" \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
        -DCMAKE_OSX_SYSROOT="$SDK" \
        -DCMAKE_INSTALL_PREFIX="$CURRENT_BUILD_DIR/install" \
        -DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF
        
    cmake --build "$CURRENT_BUILD_DIR" --config Release --target browser_network_core --parallel "$NPROC"
    create_combined_archive "$PLATFORM"
}

# 1. Build for iOS Device
build_for_platform "iOS" "arm64" "iphoneos"

# 2. Build for iOS Simulator (arm64 + x86_64)
build_for_platform "Simulator" "arm64;x86_64" "iphonesimulator"

# 3. Create XCFramework
echo "Creating XCFramework..."
rm -rf "$OUTPUT_DIR/browser_network_core.xcframework"

xcodebuild -create-xcframework \
    -library "$BUILD_DIR/iOS/lib${PROJECT_NAME}_combined.a" \
    -library "$BUILD_DIR/Simulator/lib${PROJECT_NAME}_combined.a" \
    -output "$OUTPUT_DIR/browser_network_core.xcframework"

echo "iOS core build complete. XCFramework in $OUTPUT_DIR"
