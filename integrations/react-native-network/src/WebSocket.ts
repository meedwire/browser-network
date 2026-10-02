declare const atob: (data: string) => string;
declare const btoa: (data: string) => string;

interface BrowserNetworkProxy {
  nativeWebSocketConnect(
    url: string,
    protocols: string[],
    options: {
      onOpen?: (event: { protocol: string }) => void;
      onMessage?: (event: {
        data: string;
        type: 'text' | 'binary';
        encoding?: 'utf8' | 'base64';
      }) => void;
      onError?: (event: { message: string }) => void;
      onClose?: (event: {
        code: number;
        reason: string;
        wasClean: boolean;
      }) => void;
    }
  ): number;
  nativeWebSocketSend(
    connectionId: number,
    data: string,
    options?: {
      binary?: boolean;
      base64?: boolean;
    }
  ): void;
  nativeWebSocketClose(
    connectionId: number,
    code?: number,
    reason?: string
  ): void;
}

declare global {
  var __BrowserNetworkProxy: BrowserNetworkProxy;
}

enum WebSocketState {
  CONNECTING = 0,
  OPEN = 1,
  CLOSING = 2,
  CLOSED = 3,
}

interface WebSocketEvent {
  type: string;
  target: WebSocket;
  currentTarget: WebSocket;
  bubbles: boolean;
  cancelable: boolean;
  defaultPrevented: boolean;
  composed: boolean;
  timeStamp: number;
  isTrusted: boolean;
}

interface WebSocketCloseEvent extends WebSocketEvent {
  code: number;
  reason: string;
  wasClean: boolean;
}

interface WebSocketMessageEvent extends WebSocketEvent {
  data: any;
  origin: string;
  lastEventId: string;
  source: any;
  ports: any[];
}

interface WebSocketErrorEvent extends WebSocketEvent {
  message: string;
  error?: any;
}

function bytesToBase64(bytes: Uint8Array): string {
  let binary = '';
  const chunkSize = 0x8000;

  for (let i = 0; i < bytes.length; i += chunkSize) {
    const slice = bytes.subarray(i, i + chunkSize);
    for (let j = 0; j < slice.length; j++) {
      binary += String.fromCharCode(slice[j]!);
    }
  }

  return btoa(binary);
}

function base64ToBytes(base64: string): Uint8Array {
  const binary = atob(base64);
  const bytes = new Uint8Array(binary.length);

  for (let i = 0; i < binary.length; i++) {
    bytes[i] = binary.charCodeAt(i);
  }

  return bytes;
}

function toUint8Array(data: ArrayBuffer | ArrayBufferView): Uint8Array {
  if (data instanceof ArrayBuffer) {
    return new Uint8Array(data);
  }

  return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
}

export class WebSocket {
  static CONNECTING = 0;
  static OPEN = 1;
  static CLOSING = 2;
  static CLOSED = 3;

  private _url: string;
  private _protocol = '';
  private _readyState: number = WebSocketState.CONNECTING;
  private _connectionId = -1;
  private _binaryType: 'blob' | 'arraybuffer' = 'blob';
  private _listeners: Map<string, Set<Function>> = new Map();

  public onopen: ((this: WebSocket, ev: WebSocketEvent) => any) | null = null;
  public onmessage:
    | ((this: WebSocket, ev: WebSocketMessageEvent) => any)
    | null = null;
  public onerror: ((this: WebSocket, ev: WebSocketEvent) => any) | null = null;
  public onclose: ((this: WebSocket, ev: WebSocketCloseEvent) => any) | null =
    null;

  constructor(url: string, protocols?: string | string[]) {
    this._url = url;

    let protocolsArray: string[] = [];
    if (typeof protocols === 'string') {
      protocolsArray = [protocols];
    } else if (Array.isArray(protocols)) {
      protocolsArray = protocols;
    }

    this._connect(protocolsArray);
  }

  get url(): string {
    return this._url;
  }

  get readyState(): number {
    return this._readyState;
  }

  get bufferedAmount(): number {
    return 0;
  }

  get protocol(): string {
    return this._protocol;
  }

  get binaryType(): 'blob' | 'arraybuffer' {
    return this._binaryType;
  }

  set binaryType(value: 'blob' | 'arraybuffer') {
    this._binaryType = value;
  }

  get extensions(): string {
    return '';
  }

  close(code?: number, reason?: string): void {
    if (
      this._readyState === WebSocketState.CLOSED ||
      this._readyState === WebSocketState.CLOSING
    ) {
      return;
    }

    this._readyState = WebSocketState.CLOSING;

    if (this._connectionId !== -1 && global.__BrowserNetworkProxy) {
      global.__BrowserNetworkProxy.nativeWebSocketClose(
        this._connectionId,
        code,
        reason
      );
    } else {
      this._readyState = WebSocketState.CLOSED;
    }
  }

  send(data: string | ArrayBuffer | Blob | ArrayBufferView): void {
    if (this._readyState !== WebSocketState.OPEN) {
      throw new Error('WebSocket is not open: readyState ' + this._readyState);
    }

    if (!global.__BrowserNetworkProxy || this._connectionId === -1) {
      return;
    }

    if (typeof data === 'string') {
      global.__BrowserNetworkProxy.nativeWebSocketSend(this._connectionId, data);
      return;
    }

    const BlobCtor = (globalThis as any).Blob;
    if (typeof BlobCtor === 'function' && data instanceof BlobCtor) {
      throw new TypeError('Blob is not supported in WebSocket.send() for V1.');
    }

    const bytes = toUint8Array(data as ArrayBuffer | ArrayBufferView);
    global.__BrowserNetworkProxy.nativeWebSocketSend(
      this._connectionId,
      bytesToBase64(bytes),
      { binary: true, base64: true }
    );
  }

  addEventListener(type: string, listener: Function): void {
    if (!this._listeners.has(type)) {
      this._listeners.set(type, new Set());
    }
    this._listeners.get(type)!.add(listener);
  }

  removeEventListener(type: string, listener: Function): void {
    const listeners = this._listeners.get(type);
    if (listeners) {
      listeners.delete(listener);
    }
  }

  dispatchEvent(event: WebSocketEvent): boolean {
    const listeners = this._listeners.get(event.type);
    if (listeners) {
      listeners.forEach((listener) => {
        try {
          listener.call(this, event);
        } catch {
          // Ignore listener exceptions to match browser dispatch behavior.
        }
      });
    }
    return !event.defaultPrevented;
  }

  private _createEvent(type: string): WebSocketEvent {
    return {
      type,
      target: this,
      currentTarget: this,
      bubbles: false,
      cancelable: false,
      defaultPrevented: false,
      composed: false,
      timeStamp: Date.now(),
      isTrusted: true,
    };
  }

  private _decodeBinaryMessage(base64: string): ArrayBuffer | Blob {
    const bytes = base64ToBytes(base64);
    const copy = new Uint8Array(bytes.byteLength);
    copy.set(bytes);

    if (this._binaryType === 'arraybuffer') {
      return copy.buffer;
    }

    const BlobCtor = (globalThis as any).Blob;
    if (typeof BlobCtor === 'function') {
      return new BlobCtor([copy.buffer]);
    }

    return copy.buffer;
  }

  private _connect(protocols: string[]) {
    if (!global.__BrowserNetworkProxy) {
      console.warn(
        'BrowserNetwork JSI module not found. WebSocket will not work.'
      );
      return;
    }

    try {
      this._connectionId = global.__BrowserNetworkProxy.nativeWebSocketConnect(
        this._url,
        protocols,
        {
          onOpen: (nativeEvent) => {
            this._readyState = WebSocketState.OPEN;
            this._protocol = nativeEvent.protocol || '';
            const event = this._createEvent('open');

            if (this.onopen) {
              this.onopen.call(this, event);
            }
            this.dispatchEvent(event);
          },
          onMessage: (nativeEvent) => {
            const event: WebSocketMessageEvent = {
              ...this._createEvent('message'),
              data:
                nativeEvent.type === 'binary' &&
                nativeEvent.encoding === 'base64'
                  ? this._decodeBinaryMessage(nativeEvent.data)
                  : nativeEvent.data,
              origin: this._getOrigin(),
              lastEventId: '',
              source: null,
              ports: [],
            };

            if (this.onmessage) {
              this.onmessage.call(this, event);
            }
            this.dispatchEvent(event);
          },
          onError: (nativeEvent) => {
            const event: WebSocketErrorEvent = {
              ...this._createEvent('error'),
              message: nativeEvent.message,
              error: new Error(nativeEvent.message),
            };

            if (this.onerror) {
              this.onerror.call(this, event);
            }
            this.dispatchEvent(event);
          },
          onClose: (nativeEvent) => {
            this._readyState = WebSocketState.CLOSED;
            const event: WebSocketCloseEvent = {
              ...this._createEvent('close'),
              code: nativeEvent.code,
              reason: nativeEvent.reason,
              wasClean: nativeEvent.wasClean,
            };

            if (this.onclose) {
              this.onclose.call(this, event);
            }
            this.dispatchEvent(event);
          },
        }
      );
    } catch (error: any) {
      this._readyState = WebSocketState.CLOSED;
      setTimeout(() => {
        const event = {
          ...this._createEvent('error'),
          message: error?.message || 'Connection failed',
          error:
            error instanceof Error
              ? error
              : new Error(error?.message || 'Connection failed'),
        } as WebSocketErrorEvent;

        if (this.onerror) {
          this.onerror.call(this, event);
        }
        this.dispatchEvent(event);
      }, 0);
    }
  }

  private _getOrigin(): string {
    try {
      const match = this._url.match(/^(([^:/?#]+):)?(\/\/([^/?#]*))?/);
      return match ? match[0] || '' : '';
    } catch {
      return '';
    }
  }
}
