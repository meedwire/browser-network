# @meedwire/browser-network

Browser-like networking primitives for React Native, backed by the Browser
Network runtime. The package exposes `fetch`, `Headers`, `Request`, `Response`,
`AbortController`, `AbortSignal`, `EventSource`, and `WebSocket`.

## Installation

```sh
npm install @meedwire/browser-network
cd ios && pod install
```

## Usage

```ts
import {
  AbortController,
  EventSource,
  Headers,
  Request,
  Response,
  WebSocket,
  fetch,
  setup,
} from '@meedwire/browser-network';

setup();

const controller = new AbortController();
const response = await fetch('https://example.com/health', {
  signal: controller.signal,
});
const bytes = await response.arrayBuffer();

const events = new EventSource('https://example.com/events');
const socket = new WebSocket('wss://example.com/socket');
```

## Notes

- The package requires a native React Native app with the bundled native module linked.
- Importing it in environments without the native module, such as Expo Go, fails during initialization.
- `setup()` installs `fetch`, `Headers`, `Request`, `Response`, `AbortController`, `AbortSignal`, `EventSource`, and `WebSocket` on `globalThis`.
- V1 is browser-like, not browser-complete: redirects, CORS, credentials, streaming bodies, SSE reconnect, and WebSocket binary frames are covered.
- Current known gaps for V1 are repeated response headers (`Set-Cookie` included) and `Blob` support in `WebSocket.send()`.

## Contributing

- [Development workflow](CONTRIBUTING.md#development-workflow)
- [Sending a pull request](CONTRIBUTING.md#sending-a-pull-request)
- [Code of conduct](CODE_OF_CONDUCT.md)

## License

MIT
