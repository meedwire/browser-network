type AbortListener = () => void;

function createAbortError(reason?: unknown): Error {
  const message =
    typeof reason === 'string' && reason.length > 0
      ? reason
      : 'The operation was aborted.';
  const error = new Error(message);
  error.name = 'AbortError';
  return error;
}

export class AbortSignal {
  onabort: ((this: AbortSignal, event: { type: 'abort' }) => void) | null =
    null;

  private _aborted = false;
  private _reason: unknown;
  private _listeners: Set<AbortListener> = new Set();

  get aborted(): boolean {
    return this._aborted;
  }

  get reason(): unknown {
    return this._reason;
  }

  addEventListener(type: 'abort', listener: AbortListener): void {
    if (type === 'abort') {
      this._listeners.add(listener);
    }
  }

  removeEventListener(type: 'abort', listener: AbortListener): void {
    if (type === 'abort') {
      this._listeners.delete(listener);
    }
  }

  throwIfAborted(): void {
    if (this._aborted) {
      throw createAbortError(this._reason);
    }
  }

  static abort(reason?: unknown): AbortSignal {
    const controller = new AbortController();
    controller.abort(reason);
    return controller.signal;
  }

  _signalAbort(reason?: unknown): void {
    if (this._aborted) {
      return;
    }

    this._aborted = true;
    this._reason = reason;

    const event = { type: 'abort' as const };
    if (this.onabort) {
      this.onabort.call(this, event);
    }

    this._listeners.forEach((listener) => {
      listener();
    });
  }
}

export class AbortController {
  readonly signal = new AbortSignal();

  abort(reason?: unknown): void {
    this.signal._signalAbort(reason);
  }
}
