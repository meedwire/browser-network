#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-/Users/admin/Library/Android/sdk/ndk/27.0.12077973}"
PROJECT_ROOT="$REPO_ROOT/cpp"
OUTPUT_DIR="$REPO_ROOT/integrations/react-native-network/android/libs"
BUILD_DIR="$REPO_ROOT/build/android_core"
ABIS=("arm64-v8a" "armeabi-v7a" "x86_64")

# Detect CPU cores for parallel build
NPROC=$(sysctl -n hw.ncpu || nproc || echo 4)
echo "Using $NPROC CPU cores for build"

# Check NDK
if [ ! -d "$ANDROID_NDK_HOME" ]; then
    echo "Error: Android NDK not found at $ANDROID_NDK_HOME"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

for ABI in "${ABIS[@]}"; do
    echo "----------------------------------------"
    echo "Building for $ABI..."
    echo "----------------------------------------"
    
    ABI_BUILD_DIR="$BUILD_DIR/$ABI"
    mkdir -p "$ABI_BUILD_DIR"
    
    # Use standard Unix Makefiles
    cmake \
        -H"$PROJECT_ROOT" \
        -B"$ABI_BUILD_DIR" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_NDK="$ANDROID_NDK_HOME" \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
        -DANDROID_PLATFORM=android-24 \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=20 \
        -DENABLE_TESTING=OFF

    cmake --build "$ABI_BUILD_DIR" --target browser_network_core --parallel "$NPROC"

    # Copy artifacts
    rm -rf "$OUTPUT_DIR/$ABI"
    mkdir -p "$OUTPUT_DIR/$ABI"
    cp "$ABI_BUILD_DIR/libbrowser_network_core.so" "$OUTPUT_DIR/$ABI/"
    find "$ABI_BUILD_DIR" -type f -name '*.a' -exec cp {} "$OUTPUT_DIR/$ABI/" \;
    
    echo "Finished $ABI"
done

echo "Android core build complete. Artifacts in $OUTPUT_DIR"
