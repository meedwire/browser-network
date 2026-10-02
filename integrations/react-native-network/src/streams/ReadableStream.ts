export class DirectReadableStream<R = any> {
  locked = false;
  private controller!: ReadableStreamDefaultController<R>;
  private reader: ReadableStreamDefaultReader<R> | undefined;

  constructor(
    underlyingSource: UnderlyingSource<R>,
    strategy?: QueuingStrategy<R>
  ) {
    this.controller = new ReadableStreamDefaultController(
      this,
      underlyingSource,
      strategy
    );
  }

  getReader(): ReadableStreamDefaultReader<R> {
    if (this.locked) {
      throw new Error('ReadableStream is locked.');
    }
    this.locked = true;
    this.reader = new ReadableStreamDefaultReader(this);
    return this.reader;
  }

  cancel(reason?: any): Promise<void> {
    return this.controller.cancel(reason);
  }
}

export class ReadableStreamDefaultController<R = any> {
  desiredSize: number | null = null;
  private queue: R[] = [];
  private pullAlgorithm: (
    controller: ReadableStreamDefaultController<R>
  ) => Promise<void> | void;
  private cancelAlgorithm: (reason?: any) => Promise<void> | void;
  private started = false;
  private closed = false;
  private requests: {
    resolve: (value: CreateReadableStreamChunk<R>) => void;
    reject: (reason: any) => void;
  }[] = [];

  constructor(
    _stream: DirectReadableStream<R>,
    underlyingSource: UnderlyingSource<R>,
    _strategy?: QueuingStrategy<R>
  ) {
    this.pullAlgorithm =
      underlyingSource.pull?.bind(underlyingSource) || (() => {});
    this.cancelAlgorithm =
      underlyingSource.cancel?.bind(underlyingSource) || (() => {});

    // Start logic
    if (underlyingSource.start) {
      Promise.resolve(underlyingSource.start(this)).then(() => {
        this.started = true;
        this.processPull();
      });
    } else {
      this.started = true;
      this.processPull();
    }
  }

  get closeRequested() {
    return this.closed;
  }

  enqueue(chunk: R): void {
    if (this.closed) throw new Error('Stream is closed');

    if (this.requests.length > 0) {
      const req = this.requests.shift();
      req!.resolve({ value: chunk, done: false });
    } else {
      this.queue.push(chunk);
    }
  }

  close(): void {
    if (this.closed) return;
    this.closed = true;

    while (this.requests.length > 0) {
      const req = this.requests.shift();
      req!.resolve({ value: undefined, done: true });
    }
  }

  error(e?: any): void {
    if (this.closed) return;
    this.closed = true;
    while (this.requests.length > 0) {
      const req = this.requests.shift();
      req!.reject(e);
    }
  }

  // Internal method called by reader
  _read(): Promise<CreateReadableStreamChunk<R>> {
    if (this.queue.length > 0) {
      const chunk = this.queue.shift();
      return Promise.resolve({ value: chunk!, done: false });
    }
    if (this.closed) {
      return Promise.resolve({ value: undefined, done: true });
    }

    return new Promise((resolve, reject) => {
      this.requests.push({ resolve, reject });
      this.processPull();
    });
  }

  private processPull() {
    if (this.started && !this.closed) {
      // Simple pull invocation (basic support)
      this.pullAlgorithm(this);
    }
  }

  cancel(reason?: any): Promise<void> {
    this.closed = true;
    this.queue = [];
    return Promise.resolve(this.cancelAlgorithm(reason));
  }
}

type CreateReadableStreamChunk<R> =
  | { value: R; done: false }
  | { value: undefined; done: true };

export class ReadableStreamDefaultReader<R = any> {
  closed: Promise<void>;
  private _closedResolve!: () => void;
  private _closedReject!: (r: any) => void;

  constructor(private stream: DirectReadableStream<R>) {
    this.closed = new Promise((resolve, reject) => {
      this._closedResolve = resolve;
      this._closedReject = reject;
    });
  }

  read(): Promise<CreateReadableStreamChunk<R>> {
    const controller = (this.stream as any)
      .controller as ReadableStreamDefaultController<R>;
    return controller
      ._read()
      .then((result) => {
        if (result.done) this._closedResolve();
        return result;
      })
      .catch((e) => {
        this._closedReject(e);
        throw e;
      });
  }

  releaseLock(): void {
    if (!this.stream.locked) return;
    this.stream.locked = false;
    (this.stream as any).reader = undefined;
  }

  cancel(reason?: any): Promise<void> {
    return this.stream.cancel(reason);
  }
}

// Minimal type definitions if global types are missing
interface UnderlyingSource<R = any> {
  start?: (
    controller: ReadableStreamDefaultController<R>
  ) => void | Promise<void>;
  pull?: (
    controller: ReadableStreamDefaultController<R>
  ) => void | Promise<void>;
  cancel?: (reason?: any) => void | Promise<void>;
  type?: undefined;
  autoAllocateChunkSize?: number;
}

interface QueuingStrategy<R = any> {
  highWaterMark?: number;
  size?: (chunk: R) => number;
}
