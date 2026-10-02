#import "Network.h"
#import "BrowserNetworkJSI.h"

#import <React/RCTBridgeModule.h>
#import <ReactCommon/RCTTurboModuleWithJSIBindings.h>

using namespace facebook;

@interface Network () <RCTTurboModuleWithJSIBindings>

- (NSNumber *)installWithBundleId:(NSString *)bundleId;
- (NSString *)resolvedBundleId;
- (NSString *)resolvedCookieStoragePath;

@end

@implementation Network
{
  jsi::Runtime *_jsRuntime;
  std::shared_ptr<react::CallInvoker> _jsCallInvoker;
}

RCT_EXPORT_MODULE()

+ (BOOL)requiresMainQueueSetup
{
  return NO;
}

- (NSString *)resolvedBundleId
{
  return NSBundle.mainBundle.bundleIdentifier ?: @"";
}

- (NSNumber *)installWithBundleId:(NSString *)bundleId
{
  if (_jsRuntime == nullptr || !_jsCallInvoker) {
    return @NO;
  }

  NSString *resolvedBundleId = bundleId.length > 0 ? bundleId : [self resolvedBundleId];
  NSString *resolvedCookieStoragePath = [self resolvedCookieStoragePath];
  browsernetwork::install(
      *_jsRuntime,
      _jsCallInvoker,
      resolvedBundleId.UTF8String ?: "",
      resolvedCookieStoragePath.UTF8String ?: "");
  return @YES;
}

- (NSString *)resolvedCookieStoragePath
{
  NSFileManager *fileManager = [NSFileManager defaultManager];
  NSURL *baseURL = [fileManager URLsForDirectory:NSApplicationSupportDirectory
                                       inDomains:NSUserDomainMask].firstObject;

  if (baseURL == nil) {
    baseURL = [fileManager URLsForDirectory:NSLibraryDirectory
                                  inDomains:NSUserDomainMask].firstObject;
  }

  NSURL *cookiesDirectoryURL =
      [baseURL URLByAppendingPathComponent:@"browser-network" isDirectory:YES];
  NSURL *cookieFileURL =
      [cookiesDirectoryURL URLByAppendingPathComponent:@"cookies.txt" isDirectory:NO];
  return cookieFileURL.path ?: @"";
}

- (void)installJSIBindingsWithRuntime:(facebook::jsi::Runtime &)runtime
                          callInvoker:(const std::shared_ptr<facebook::react::CallInvoker> &)callInvoker
{
  _jsRuntime = &runtime;
  _jsCallInvoker = callInvoker;
  (void)[self installWithBundleId:[self resolvedBundleId]];
}

- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params
{
  if (params.jsRuntime != nullptr) {
    _jsRuntime = static_cast<jsi::Runtime *>(params.jsRuntime);
  }
  _jsCallInvoker = params.jsCallInvoker;
  (void)[self installWithBundleId:[self resolvedBundleId]];
  return std::make_shared<facebook::react::NativeNetworkSpecJSI>(params);
}

RCT_EXPORT_SYNCHRONOUS_TYPED_METHOD(NSNumber *, install : (NSString *)bundleId)
{
  return [self installWithBundleId:bundleId];
}

- (NSDictionary *)constantsToExport
{
  return @{@"packageName" : [self resolvedBundleId]};
}

@end
