# Architecture Documentation

## Overview

This project follows **Clean Architecture** principles to ensure robustness, testability, and adherence to SOLID principles. The core logic is implemented in C++ to share code between Android and iOS, exposed to React Native via JSI.

## Layer Structure

The project is organized into concentric layers, with dependencies pointing **inwards**.

### 1. Core (`cpp/core/`)

- **Role:** Enterprise Business Rules / Entities.
- **Dependencies:** None. Pure C++.
- **Contents:**
  - Request/Response Models.
  - Enums (Methods, Status Codes).
  - Parsing Logic rules.

### 2. Ports (`cpp/ports/`)

- **Role:** Interface Adapters (Abstract Base Classes).
- **Dependencies:** `Core`.
- **Contents:**
  - `INetworkAdapter`: Interface for performing HTTP requests.
  - `IPersistenceAdapter`: Interface for caching/cookies (if needed).
  - `ILogger`: Interface for logging.

### 3. Usecases (`cpp/usecases/`)

- **Role:** Application Business Rules.
- **Dependencies:** `Core`, `Ports`.
- **Contents:**
  - `SendRequestUseCase`: Orchestrates the flow of validating a request and sending it via the `INetworkAdapter`.
  - `CancelRequestUseCase`: Logic to abort a request.

### 4. Adapters (`cpp/adapters/`)

- **Role:** Frameworks & Drivers. Implementations of `Ports`.
- **Dependencies:** `Ports`, `Core`, Platform SDKs.
- **Contents:**
  - `adapters/ios/`: Objective-C++ implementation of `INetworkAdapter` using `NSURLSession`.
  - `adapters/android/`: C++ JNI implementation calling Java `OkHttp` or `HttpUrlConnection`.
  - `adapters/curl/`: (Optional) `libcurl` implementation for testing or other platforms.

### 5. JSI Layer (`cpp/jsi/`)

- **Role:** Interface/Presentation.
- **Dependencies:** `Usecases`, React Native JSI.
- **Contents:**
  - Exposes C++ methods to JavaScript.
  - Converts JS Objects <-> C++ Core Entities.

## Development Rules

1.  **Dependency Rule:** Source code dependencies must only point inward. `Core` knows nothing about `Adapters`.
2.  **Abstractions:** Use `Ports` for any external interaction (Network, File System, Logging).
3.  **Testing:**
    - Write **Unit Tests** for `Core` and `Usecases` using pure C++ mocks.
    - Write **Integration Tests** for `Adapters` on their respective platforms.

## Build System

- Uses `CMake` for C++ compilation.
- Android uses Gradle to invoke CMake.
- iOS uses CocoaPods/Xcode which includes the C++ source.
