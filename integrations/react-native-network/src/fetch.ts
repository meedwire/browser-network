import {
  DirectReadableStream,
  ReadableStreamDefaultController,
} from './streams/ReadableStream';

type HeadersInit_ = NetworkHeaders | string[][] | Record<string, string>;
type RequestCredentials_ = 'omit' | 'same-origin' | 'include';
type AbortSignalLike = {
  aborted: boolean;
  reason?: unknown;
  addEventListener?: (type: 'abort', listener: () => void) => void;
  removeEventListener?: (type: 'abort', listener: () => void) => void;
};
type FormDataLike = {
  entries?: () => IterableIterator<[string, unknown]>;
  _parts?: Array<[string, unknown]>;
};
type BinaryBodyInit_ = ArrayBuffer | ArrayBufferView | Uint8Array;
type BodyInit_ =
  | string
  | URLSearchParams
  | Uint8Array
  | ArrayBuffer
  | ArrayBufferView
  | FormDataLike
  | null;

interface ResponseInit_ {
  status?: number;
  statusText?: string;
  headers?: HeadersInit_;
  url?: string;
}

interface RequestInit {
  method?: string;
  headers?: HeadersInit_;
  body?: BodyInit_;
  redirect?: 'follow' | 'error' | 'manual';
  credentials?: RequestCredentials_;
  signal?: AbortSignalLike | null;
}

declare const browserNetwork: {
  nativePerformRequest: (
    url: string,
    options: {
      method?: string;
      headers?: Record<string, string>;
      body?: string;
      bodyBase64?: boolean;
      redirect?: string;
      credentials?: RequestCredentials_;
      onResponse: (response: {
        status: number;
        url: string;
        headers: Record<string, string>;
        body: string;
      }) => void;
      onError: (error: string) => void;
    }
  ) => void;
  nativePerformStreamRequest: (
    url: string,
    options: {
      method?: string;
      headers?: Record<string, string>;
      body?: string;
      bodyBase64?: boolean;
      redirect?: string;
      credentials?: RequestCredentials_;
      onResponse: (response: {
        status: number;
        url: string;
        redirected: boolean;
        headers: Record<string, string>;
      }) => void;
      onData: (chunk: string | Uint8Array) => void;
      onEnd: () => void;
      onError: (error: string) => void;
    }
  ) => number;
  nativeAbortStreamRequest: (requestId: number) => boolean;
};

declare const atob: (data: string) => string;
declare const btoa: (data: string) => string;
declare const TextDecoder:
  | undefined
  | {
      new (encoding?: string): {
        decode(data: Uint8Array): string;
      };
    };
declare const TextEncoder:
  | undefined
  | {
      new (): {
        encode(data: string): Uint8Array;
      };
    };
declare const FormData: undefined | { prototype: FormDataLike };

function createAbortError(reason?: unknown): Error {
  const message =
    typeof reason === 'string' && reason.length > 0
      ? reason
      : 'The operation was aborted.';
  const error = new Error(message);
  error.name = 'AbortError';
  return error;
}

function encodeUtf8(value: string): Uint8Array {
  if (typeof TextEncoder !== 'undefined') {
    return new TextEncoder().encode(value);
  }

  const encoded = unescape(encodeURIComponent(value));
  const bytes = new Uint8Array(encoded.length);
  for (let i = 0; i < encoded.length; i++) {
    bytes[i] = encoded.charCodeAt(i);
  }
  return bytes;
}

function decodeUtf8(value: Uint8Array): string {
  if (typeof TextDecoder !== 'undefined') {
    return new TextDecoder('utf-8').decode(value);
  }

  let result = '';
  for (let i = 0; i < value.length; i++) {
    result += String.fromCharCode(value[i]!);
  }
  try {
    return decodeURIComponent(escape(result));
  } catch {
    return result;
  }
}

function concatUint8Arrays(chunks: Uint8Array[]): Uint8Array {
  const totalLength = chunks.reduce((sum, chunk) => sum + chunk.length, 0);
  const result = new Uint8Array(totalLength);
  let offset = 0;

  for (const chunk of chunks) {
    result.set(chunk, offset);
    offset += chunk.length;
  }

  return result;
}

function toUint8Array(body: BinaryBodyInit_): Uint8Array {
  if (body instanceof Uint8Array) {
    return body;
  }

  if (body instanceof ArrayBuffer) {
    return new Uint8Array(body);
  }

  return new Uint8Array(body.buffer, body.byteOffset, body.byteLength);
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

function isBinaryBody(body: unknown): body is BinaryBodyInit_ {
  return (
    body instanceof Uint8Array ||
    body instanceof ArrayBuffer ||
    ArrayBuffer.isView(body)
  );
}

function isFormDataBody(body: unknown): body is FormDataLike {
  const FormDataCtor =
    typeof FormData !== 'undefined'
      ? (FormData as unknown as { new (): unknown })
      : undefined;

  if (FormDataCtor && body instanceof FormDataCtor) {
    return true;
  }

  return Boolean(
    body &&
      typeof body === 'object' &&
      (typeof (body as FormDataLike).entries === 'function' ||
        Array.isArray((body as FormDataLike)._parts))
  );
}

function getHeaderValueCaseInsensitive(
  headers: Record<string, string>,
  name: string
): string | undefined {
  const target = name.toLowerCase();
  return Object.entries(headers).find(([key]) => key.toLowerCase() === target)?.[1];
}

function isForbiddenHeaderName(name: string): boolean {
  const normalized = name.toLowerCase();
  return (
    normalized === 'cookie' ||
    normalized === 'cookie2' ||
    normalized === 'set-cookie' ||
    normalized === 'set-cookie2'
  );
}

function serializeFormData(
  body: FormDataLike,
  headers: Record<string, string>
): string {
  let entries: Array<[string, unknown]> = [];

  if (typeof body.entries === 'function') {
    entries = Array.from(body.entries());
  } else if (Array.isArray(body._parts)) {
    entries = body._parts;
  }

  const boundary = `----browser-network-${Math.random()
    .toString(16)
    .slice(2)}`;
  const parts: string[] = [];

  for (const [name, value] of entries) {
    if (typeof value !== 'string') {
      throw new TypeError(
        'Only text FormData fields are supported in this V1 runtime.'
      );
    }

    parts.push(`--${boundary}\r\n`);
    parts.push(`Content-Disposition: form-data; name="${name}"\r\n\r\n`);
    parts.push(value);
    parts.push('\r\n');
  }

  parts.push(`--${boundary}--\r\n`);

  if (!getHeaderValueCaseInsensitive(headers, 'Content-Type')) {
    headers['content-type'] = `multipart/form-data; boundary=${boundary}`;
  }

  return parts.join('');
}

function normalizeRequestBody(
  body: BodyInit_ | undefined,
  headers: Record<string, string>
): { body?: string; bodyBase64?: boolean } {
  if (body == null) {
    return {};
  }

  if (typeof body === 'string') {
    return { body };
  }

  if (body instanceof URLSearchParams) {
    if (!getHeaderValueCaseInsensitive(headers, 'Content-Type')) {
      headers['content-type'] =
        'application/x-www-form-urlencoded;charset=UTF-8';
    }
    return { body: body.toString() };
  }

  if (isBinaryBody(body)) {
    return {
      body: bytesToBase64(toUint8Array(body)),
      bodyBase64: true,
    };
  }

  if (isFormDataBody(body)) {
    return { body: serializeFormData(body, headers) };
  }

  return { body: String(body) };
}

export class NetworkHeaders {
  private map: Map<string, string> = new Map();

  constructor(init?: HeadersInit_) {
    if (init) {
      if (init instanceof NetworkHeaders) {
        init.forEach((value, key) => this.set(key, value));
      } else if (Array.isArray(init)) {
        init.forEach(([key, value]) => {
          if (key && value) {
            this.set(key, value);
          }
        });
      } else {
        Object.entries(init).forEach(([key, value]) => {
          if (value !== undefined) {
            this.set(key, String(value));
          }
        });
      }
    }
  }

  append(name: string, value: string): void {
    if (isForbiddenHeaderName(name)) {
      return;
    }
    const existing = this.get(name);
    this.set(name, existing ? `${existing}, ${value}` : value);
  }

  delete(name: string): void {
    this.map.delete(name.toLowerCase());
  }

  get(name: string): string | null {
    return this.map.get(name.toLowerCase()) || null;
  }

  has(name: string): boolean {
    return this.map.has(name.toLowerCase());
  }

  set(name: string, value: string): void {
    if (isForbiddenHeaderName(name)) {
      return;
    }
    this.map.set(name.toLowerCase(), value);
  }

  forEach(
    callback: (value: string, key: string, parent: NetworkHeaders) => void
  ): void {
    this.map.forEach((value, key) => callback(value, key, this));
  }
}

export class Response {
  public ok: boolean;
  public status: number;
  public statusText: string;
  public headers: NetworkHeaders;
  public url: string;
  public type:
    | 'basic'
    | 'cors'
    | 'default'
    | 'error'
    | 'opaque'
    | 'opaqueredirect'
    | 'websocket';
  public redirected: boolean;
  private _body: BodyInit_ | null | DirectReadableStream<Uint8Array>;
  private _bodyUsed = false;

  constructor(
    body: BodyInit_ | null | DirectReadableStream<Uint8Array>,
    init?: ResponseInit_
  ) {
    this._body = body;
    this.status = init?.status ?? 200;
    this.statusText = init?.statusText ?? 'OK';
    this.ok = this.status >= 200 && this.status < 300;
    this.headers = new NetworkHeaders(init?.headers);
    this.type = (init as any)?.type ?? 'default';
    this.url = init?.url ?? '';
    this.redirected = (init as any)?.redirected ?? false;
  }

  get body(): DirectReadableStream<Uint8Array> | null {
    if (this._body instanceof DirectReadableStream) {
      return this._body as DirectReadableStream<Uint8Array>;
    }
    return null;
  }

  get bodyUsed(): boolean {
    return this._bodyUsed;
  }

  private async readBodyBytes(): Promise<Uint8Array> {
    if (this._bodyUsed) {
      throw new TypeError('Body has already been consumed.');
    }

    this._bodyUsed = true;

    if (this._body instanceof DirectReadableStream) {
      const reader = this._body.getReader();
      const chunks: Uint8Array[] = [];

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) {
          chunks.push(value);
        }
      }

      return concatUint8Arrays(chunks);
    }

    if (typeof this._body === 'string') {
      return encodeUtf8(this._body);
    }

    if (this._body == null) {
      return new Uint8Array(0);
    }

    if (isBinaryBody(this._body)) {
      return toUint8Array(this._body);
    }

    return encodeUtf8(JSON.stringify(this._body));
  }

  async text(): Promise<string> {
    const bytes = await this.readBodyBytes();
    return decodeUtf8(bytes);
  }

  async json(): Promise<any> {
    const text = await this.text();
    return JSON.parse(text);
  }

  async arrayBuffer(): Promise<ArrayBuffer> {
    const bytes = await this.readBodyBytes();
    const copy = new Uint8Array(bytes.byteLength);
    copy.set(bytes);
    return copy.buffer;
  }
}

export class Request {
  url: string;
  method: string;
  headers: NetworkHeaders;
  body: BodyInit_ | undefined;
  redirect: 'follow' | 'error' | 'manual';
  credentials: RequestCredentials_;
  signal?: AbortSignalLike | null;

  constructor(input: string | Request, init?: RequestInit) {
    if (input instanceof Request) {
      this.url = input.url;
      this.method = init?.method ?? input.method;
      this.headers = new NetworkHeaders(init?.headers ?? input.headers);
      this.body = init && 'body' in init ? init.body : input.body;
      this.redirect = init?.redirect ?? input.redirect;
      this.credentials = init?.credentials ?? input.credentials;
      this.signal = init?.signal ?? input.signal;
    } else {
      this.url = input;
      this.method = init?.method ?? 'GET';
      this.headers = new NetworkHeaders(init?.headers);
      this.body = init?.body;
      this.redirect = init?.redirect ?? 'follow';
      this.credentials = init?.credentials ?? 'same-origin';
      this.signal = init?.signal;
    }
  }
}

export async function fetch(
  input: string | Request,
  init?: RequestInit
): Promise<Response> {
  const url = typeof input === 'string' ? input : input.url;

  const headersObj: Record<string, string> = {};
  const initHeaders =
    init?.headers || (input instanceof Request ? input.headers : undefined);

  if (initHeaders) {
    if (initHeaders instanceof NetworkHeaders) {
      initHeaders.forEach((val, key) => (headersObj[key] = val));
    } else if (Array.isArray(initHeaders)) {
      initHeaders.forEach((entry) => {
        const [key, val] = entry;
        if (
          key !== undefined &&
          val !== undefined &&
          !isForbiddenHeaderName(String(key))
        ) {
          headersObj[String(key)] = String(val);
        }
      });
    } else if (typeof initHeaders === 'object') {
      Object.entries(initHeaders as Record<string, unknown>).forEach(
        ([key, val]) => {
          if (
            val !== undefined &&
            val !== null &&
            !isForbiddenHeaderName(String(key))
          ) {
            headersObj[String(key)] = String(val);
          }
        }
      );
    }
  }

  const method =
    init?.method || (input instanceof Request ? input.method : 'GET');
  const body =
    init && 'body' in init
      ? init.body
      : input instanceof Request
        ? input.body
        : undefined;
  const redirect =
    init?.redirect || (input instanceof Request ? input.redirect : 'follow');
  const signal =
    init?.signal !== undefined
      ? init.signal
      : input instanceof Request
        ? input.signal
        : undefined;
  const credentials =
    init?.credentials !== undefined
      ? init.credentials
      : input instanceof Request
        ? input.credentials
        : 'same-origin';

  if (signal?.aborted) {
    throw createAbortError(signal.reason);
  }

  const normalizedBody = normalizeRequestBody(body, headersObj);

  return new Promise((resolve, reject) => {
    if (typeof browserNetwork === 'undefined') {
      reject(new Error('BrowserNetwork JSI not installed'));
      return;
    }

    let controller: ReadableStreamDefaultController<Uint8Array> | undefined;
    let settled = false;
    let streamResolved = false;
    let aborted = false;
    let requestId = -1;

    const cleanupAbortListener = () => {
      if (signal?.removeEventListener && onAbort) {
        signal.removeEventListener('abort', onAbort);
      }
    };

    const onAbort = () => {
      const error = createAbortError(signal?.reason);
      aborted = true;

      if (requestId >= 0) {
        browserNetwork.nativeAbortStreamRequest(requestId);
        requestId = -1;
      }

      if (!settled) {
        settled = true;
        cleanupAbortListener();
        reject(error);
      }

      if (controller) {
        controller.error(error);
      }
    };

    if (signal?.addEventListener) {
      signal.addEventListener('abort', onAbort);
    }

    requestId = browserNetwork.nativePerformStreamRequest(url, {
      method,
      headers: headersObj,
      body: normalizedBody.body,
      bodyBase64: normalizedBody.bodyBase64,
      redirect,
      credentials,
      onResponse: (nativeResp) => {
        if (aborted) {
          return;
        }

        const isRedirect = nativeResp.status >= 300 && nativeResp.status < 400;

        if (isRedirect && redirect === 'error') {
          settled = true;
          cleanupAbortListener();
          reject(
            new TypeError('Network request failed: Redirect detected in error mode')
          );
          return;
        }

        if (isRedirect && redirect === 'manual') {
          settled = true;
          cleanupAbortListener();
          const opaqueResponse = new Response(null, {
            status: 0,
            statusText: '',
            headers: {},
            url: nativeResp.url,
          });
          (opaqueResponse as any).type = 'opaqueredirect';
          (opaqueResponse as any).redirected = false;
          resolve(opaqueResponse);
          return;
        }

        const stream = new DirectReadableStream<Uint8Array>({
          start(c) {
            controller = c;
          },
          cancel(reason) {
            aborted = true;
            if (requestId >= 0) {
              browserNetwork.nativeAbortStreamRequest(requestId);
              requestId = -1;
            }
            cleanupAbortListener();
            return Promise.resolve(reason);
          },
        });

        const response = new Response(stream, {
          status: nativeResp.status,
          statusText:
            nativeResp.status >= 200 && nativeResp.status < 300 ? 'OK' : 'Error',
          headers: nativeResp.headers,
          url: nativeResp.url,
        } as any);

        (response as any).redirected = nativeResp.redirected;
        streamResolved = true;
        settled = true;
        resolve(response);
      },
      onData: (chunk: string | Uint8Array) => {
        if (aborted || !controller) {
          return;
        }

        try {
          const data =
            typeof chunk === 'string' ? base64ToBytes(chunk) : chunk;
          controller.enqueue(data);
        } catch (error) {
          controller.error(
            error instanceof Error ? error : new Error('Failed to decode chunk')
          );
        }
      },
      onEnd: () => {
        requestId = -1;
        cleanupAbortListener();
        if (controller && !aborted) {
          controller.close();
        }
      },
      onError: (error) => {
        requestId = -1;
        cleanupAbortListener();

        if (aborted) {
          return;
        }

        const networkError = new TypeError(`Network request failed: ${error}`);

        if (controller && streamResolved) {
          controller.error(networkError);
        } else if (!settled) {
          settled = true;
          reject(networkError);
        }
      },
    });
  });
}
