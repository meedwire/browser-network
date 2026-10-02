declare var __BrowserNetworkProxy: {
  nativeSSEConnect: (
    url: string,
    options: {
      headers?: Record<string, string>;
      withCredentials?: boolean;
      onOpen?: () => void;
      onMessage?: (event: {
        type: string;
        data: string;
        lastEventId: string;
      }) => void;
      onRetry?: (retryMs: number) => void;
      onError?: (error: { message: string }) => void;
    }
  ) => number;
  nativeSSEClose: (connectionId: number) => boolean;
};

export const EventSourceReadyState = {
  CONNECTING: 0,
  OPEN: 1,
  CLOSED: 2,
} as const;

export type EventSourceReadyStateType =
  typeof EventSourceReadyState[keyof typeof EventSourceReadyState];

export interface MessageEvent {
  type: string;
  data: string;
  lastEventId: string;
  origin: string;
}

export type EventHandler = (event: MessageEvent) => void;
export type ErrorHandler = (event: Event) => void;
export type OpenHandler = (event: Event) => void;

export interface EventSourceInit {
  headers?: Record<string, string>;
  withCredentials?: boolean;
}

export class EventSource {
  static readonly CONNECTING = EventSourceReadyState.CONNECTING;
  static readonly OPEN = EventSourceReadyState.OPEN;
  static readonly CLOSED = EventSourceReadyState.CLOSED;

  readonly url: string;
  readonly withCredentials: boolean;

  private _readyState: EventSourceReadyStateType =
    EventSourceReadyState.CONNECTING;
  private _connectionId = -1;
  private _lastEventId = '';
  private _reconnectDelay = 3000;
  private _reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  private _closedByUser = false;
  private _headers: Record<string, string>;
  private _eventListeners: Map<string, Set<EventHandler>> = new Map();

  onopen: OpenHandler | null = null;
  onmessage: EventHandler | null = null;
  onerror: ErrorHandler | null = null;

  constructor(url: string, options?: EventSourceInit) {
    if (typeof __BrowserNetworkProxy === 'undefined') {
      throw new Error('BrowserNetwork JSI not installed. Call setup() first.');
    }

    this.url = url;
    this.withCredentials = options?.withCredentials ?? false;
    this._headers = { ...(options?.headers ?? {}) };

    this._connect();
  }

  get readyState(): EventSourceReadyStateType {
    return this._readyState;
  }

  private _connect(): void {
    if (this._closedByUser) {
      return;
    }

    this._clearReconnectTimer();
    this._readyState = EventSourceReadyState.CONNECTING;

    const headers = { ...this._headers };
    if (this._lastEventId !== '') {
      headers['Last-Event-ID'] = this._lastEventId;
    }

    this._connectionId = __BrowserNetworkProxy.nativeSSEConnect(this.url, {
      headers,
      withCredentials: this.withCredentials,
      onOpen: this._handleOpen.bind(this),
      onMessage: this._handleMessage.bind(this),
      onRetry: this._handleRetry.bind(this),
      onError: this._handleError.bind(this),
    });

    if (this._connectionId < 0) {
      this._readyState = EventSourceReadyState.CLOSED;
      throw new Error('Failed to establish SSE connection');
    }
  }

  private _handleOpen(): void {
    this._readyState = EventSourceReadyState.OPEN;

    const event = { type: 'open' } as Event;
    if (this.onopen) {
      this.onopen(event);
    }

    this._dispatchEvent('open', {
      type: 'open',
      data: '',
      lastEventId: this._lastEventId,
      origin: this._getOrigin(),
    });
  }

  private _handleMessage(nativeEvent: {
    type: string;
    data: string;
    lastEventId: string;
  }): void {
    if (typeof nativeEvent.lastEventId === 'string') {
      this._lastEventId = nativeEvent.lastEventId;
    }

    const type = nativeEvent.type || 'message';
    const event: MessageEvent = {
      type,
      data: nativeEvent.data,
      lastEventId: this._lastEventId,
      origin: this._getOrigin(),
    };

    this._dispatchEvent(type, event);

    if (type === 'message' && this.onmessage) {
      this.onmessage(event);
    }
  }

  private _handleRetry(retryMs: number): void {
    if (Number.isFinite(retryMs) && retryMs >= 0) {
      this._reconnectDelay = retryMs;
    }
  }

  private _handleError(nativeError: { message: string }): void {
    this._connectionId = -1;

    if (this._closedByUser) {
      this._readyState = EventSourceReadyState.CLOSED;
      return;
    }

    this._readyState = EventSourceReadyState.CONNECTING;

    const event = {
      type: 'error',
      message: nativeError.message,
    } as unknown as Event;

    if (this.onerror) {
      this.onerror(event);
    }

    this._dispatchEvent('error', {
      type: 'error',
      data: nativeError.message,
      lastEventId: this._lastEventId,
      origin: this._getOrigin(),
    });

    this._scheduleReconnect();
  }

  private _scheduleReconnect(): void {
    if (this._closedByUser || this._reconnectTimer !== null) {
      return;
    }

    this._reconnectTimer = setTimeout(() => {
      this._reconnectTimer = null;
      this._connect();
    }, this._reconnectDelay);
  }

  private _clearReconnectTimer(): void {
    if (this._reconnectTimer !== null) {
      clearTimeout(this._reconnectTimer);
      this._reconnectTimer = null;
    }
  }

  private _dispatchEvent(type: string, event: MessageEvent): void {
    const listeners = this._eventListeners.get(type);
    if (!listeners) {
      return;
    }

    listeners.forEach((handler) => {
      try {
        handler(event);
      } catch {
        // Ignore handler errors to match browser event dispatch behavior.
      }
    });
  }

  private _getOrigin(): string {
    try {
      return new URL(this.url).origin;
    } catch {
      return '';
    }
  }

  addEventListener(type: string, handler: EventHandler): void {
    if (!this._eventListeners.has(type)) {
      this._eventListeners.set(type, new Set());
    }

    this._eventListeners.get(type)!.add(handler);
  }

  removeEventListener(type: string, handler: EventHandler): void {
    const listeners = this._eventListeners.get(type);
    if (listeners) {
      listeners.delete(handler);
    }
  }

  close(): void {
    this._closedByUser = true;
    this._readyState = EventSourceReadyState.CLOSED;
    this._clearReconnectTimer();

    if (this._connectionId >= 0 && typeof __BrowserNetworkProxy !== 'undefined') {
      __BrowserNetworkProxy.nativeSSEClose(this._connectionId);
      this._connectionId = -1;
    }
  }
}
