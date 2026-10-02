package com.network

import android.os.Build
import android.util.Log
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.turbomodule.core.CallInvokerHolderImpl
import com.facebook.react.common.annotations.FrameworkAPI
import java.io.File

class NetworkModule(reactContext: ReactApplicationContext) :
  NativeNetworkSpec(reactContext) {

  override fun getName(): String {
    return NAME
  }

  override fun getConstants(): MutableMap<String, Any> {
    return hashMapOf("packageName" to reactApplicationContext.packageName)
  }

  @OptIn(FrameworkAPI::class)
  override fun install(bundleId: String): Boolean {
    val resolvedBundleId = bundleId.ifBlank { reactApplicationContext.packageName }
    val resolvedCookieStoragePath = resolveCookieStoragePath()

    if (!ensureNativeLibraryLoaded()) {
      Log.e(TAG, "Cannot install JSI: native library is unavailable for ABI=${Build.SUPPORTED_ABIS.joinToString()}")
      return false
    }

    Log.d(TAG, "NetworkModule.install($resolvedBundleId, $resolvedCookieStoragePath) called")

    val jsiPtr = reactApplicationContext.javaScriptContextHolder?.get()
    Log.d(TAG, "JSI pointer: $jsiPtr")

    val callInvokerHolder = reactApplicationContext.jsCallInvokerHolder as? CallInvokerHolderImpl
    Log.d(TAG, "CallInvokerHolder: $callInvokerHolder")

    return if (jsiPtr != null && jsiPtr != 0L && callInvokerHolder != null) {
      Log.d(TAG, "Calling nativeInstall...")
      nativeInstall(jsiPtr, callInvokerHolder, resolvedBundleId, resolvedCookieStoragePath)
      Log.d(TAG, "nativeInstall completed successfully")
      true
    } else {
      Log.e(TAG, "Cannot install JSI: jsiPtr=$jsiPtr, callInvokerHolder=$callInvokerHolder")
      false
    }
  }

  @OptIn(FrameworkAPI::class)
  private external fun nativeInstall(
    jsiPtr: Long,
    jsCallInvokerHolder: CallInvokerHolderImpl,
    bundleId: String,
    cookieStoragePath: String
  )

  private fun resolveCookieStoragePath(): String {
    return File(File(reactApplicationContext.filesDir, "browser-network"), "cookies.txt").absolutePath
  }

  private fun ensureNativeLibraryLoaded(): Boolean {
    if (isLibraryLoaded) {
      return true
    }

    return synchronized(companionLock) {
      if (isLibraryLoaded) {
        true
      } else {
        try {
          System.loadLibrary("react_native_browser_network")
          isLibraryLoaded = true
          Log.d(TAG, "react_native_browser_network library loaded successfully")
          true
        } catch (error: UnsatisfiedLinkError) {
          Log.e(TAG, "Failed to load react_native_browser_network", error)
          false
        }
      }
    }
  }

  companion object {
    const val NAME = "Network"
    const val TAG = "NetworkModule"
    private val companionLock = Any()

    @Volatile
    private var isLibraryLoaded = false
  }
}
