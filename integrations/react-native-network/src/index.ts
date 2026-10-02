import { NetworkHeaders, Request, Response, fetch } from './fetch';
import { AbortController, AbortSignal } from './abort';
import { EventSource } from './eventsource';
import { WebSocket } from './WebSocket';
import { NativeModules } from 'react-native';

const globalObject = globalThis as typeof globalThis & {
  __BrowserNetworkProxy?: unknown;
};

// Initialize JSI on module load
if (globalObject.__BrowserNetworkProxy == null) {
  if (NativeModules.Network == null) {
    throw new Error(
      'Base module not found. Did you build the native module correctly?'
    );
  }

  const constants = NativeModules.Network.getConstants?.() ?? {};
  const bundleId =
    constants.packageName ?? constants.bundleId ?? constants.appId ?? '';

  const installed = NativeModules.Network.install(bundleId);
  if (!installed) {
    throw new Error(
      'Failed to install BrowserNetwork: JSI pointer or CallInvoker not available.'
    );
  }

  if (globalObject.__BrowserNetworkProxy == null) {
    throw new Error(
      'BrowserNetwork native object is not available. JSI installation failed silently.'
    );
  }
}

/**
 * Instala os polyfills globalmente no React Native.
 *
 * Após chamar setup(), as seguintes APIs estarão disponíveis globalmente:
 * - fetch() - Fetch API polyfill
 * - Headers - Headers class
 * - Request - Request class
 * - Response - Response class
 * - AbortController / AbortSignal
 * - EventSource - Server-Sent Events polyfill
 * - WebSocket - WebSocket class
 */
export function setup() {
  if (typeof global !== 'undefined') {
    // @ts-ignore
    global.fetch = fetch;
    // @ts-ignore
    global.Headers = NetworkHeaders;
    // @ts-ignore
    global.Request = Request;
    // @ts-ignore
    global.Response = Response;
    // @ts-ignore
    if (!(global as any).AbortController) {
      (global as any).AbortController = AbortController;
    }
    // @ts-ignore
    if (!(global as any).AbortSignal) {
      (global as any).AbortSignal = AbortSignal;
    }
    // @ts-ignore
    (global as any).EventSource = EventSource;
    if (!(global as any).WebSocket) {
      (global as any).WebSocket = WebSocket;
    }
  }
}

export {
  NetworkHeaders as Headers,
  AbortController,
  AbortSignal,
  Request,
  Response,
  fetch,
  EventSource,
  WebSocket,
};
