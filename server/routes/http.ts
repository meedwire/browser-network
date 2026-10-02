/**
 * @file http.ts
 * @description Rotas HTTP/HTTPS completas para testes da biblioteca
 *
 * Implementa 100% dos recursos HTTP que um navegador oferece:
 * - Todos os métodos HTTP (GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS)
 * - Headers customizados
 * - Cookies (Set-Cookie, parsing, SameSite, Secure, HttpOnly)
 * - Redirects (301, 302, 303, 307, 308)
 * - Cache (ETag, Last-Modified, Cache-Control, 304 Not Modified)
 * - Compression (gzip, brotli, deflate)
 * - Content types (JSON, text, binary, multipart)
 * - Status codes variados
 * - Authentication (Basic, Bearer)
 * - Range requests (206 Partial Content)
 * - Conditional requests
 */

import { getCorsHeaders } from "../utils/cors";
import {
  delay,
  generateBinaryData,
  parseCookies,
  createSetCookie,
  generateETag,
  checkNotModified,
  getQueryParams,
  type CookieOptions,
} from "../utils/helpers";
import type { Server } from "bun";

function parseBooleanParam(value: string | null): boolean | undefined {
  if (value === null) {
    return undefined;
  }

  return value === "true" || value === "1";
}

function parseNumberParam(value: string | null): number | undefined {
  if (value === null) {
    return undefined;
  }

  const parsedValue = Number(value);
  return Number.isFinite(parsedValue) ? parsedValue : undefined;
}

function parseDateParam(value: string | null): Date | undefined {
  if (!value) {
    return undefined;
  }

  const parsedDate = new Date(value);
  return Number.isNaN(parsedDate.getTime()) ? undefined : parsedDate;
}

function parseSameSiteParam(value: string | null): CookieOptions["sameSite"] {
  if (value === "Strict" || value === "Lax" || value === "None") {
    return value;
  }

  return undefined;
}

function getCookieOptionsFromUrl(url: URL, defaultPath = "/"): CookieOptions {
  return {
    path: url.searchParams.get("path") || defaultPath,
    domain: url.searchParams.get("domain") || undefined,
    maxAge: parseNumberParam(url.searchParams.get("maxAge")),
    expires: parseDateParam(url.searchParams.get("expires")),
    httpOnly: parseBooleanParam(url.searchParams.get("httpOnly")),
    secure: parseBooleanParam(url.searchParams.get("secure")),
    sameSite: parseSameSiteParam(url.searchParams.get("sameSite")),
  };
}

function serializeCookieOptions(options: CookieOptions) {
  return {
    path: options.path || null,
    domain: options.domain || null,
    maxAge: options.maxAge ?? null,
    expires: options.expires?.toUTCString() || null,
    httpOnly: options.httpOnly ?? false,
    secure: options.secure ?? false,
    sameSite: options.sameSite || null,
  };
}

function createCookieInspectionResponse(req: Request): Response {
  return Response.json(
    {
      path: new URL(req.url).pathname,
      cookieHeader: req.headers.get("Cookie"),
      cookies: parseCookies(req.headers.get("Cookie")),
    },
    { headers: getCorsHeaders(req) },
  );
}

export const httpRoutes = {
  /**
   * Rota com CORS Restrito - aceita apenas a origin do app exemplo
   */
  "/cors/restricted": {
    GET: (req: Request) => {
      const origin = req.headers.get("Origin");
      console.log("Origin:", origin);
      const allowedOrigin = "https://network-example.app";

      if (origin !== allowedOrigin) {
        return new Response(
          JSON.stringify({
            success: false,
            message: `Origin ${origin} blocked. Only ${allowedOrigin} is allowed.`,
            receivedOrigin: origin,
          }),
          {
            status: 403,
            headers: { "Content-Type": "application/json" },
          },
        );
      }

      return Response.json(
        {
          success: true,
          message: "Origin accepted!",
          origin: origin,
        },
        {
          headers: {
            "Access-Control-Allow-Origin": allowedOrigin,
            "Access-Control-Allow-Methods": "GET, OPTIONS",
            "Access-Control-Allow-Headers":
              "Origin, X-Requested-With, Content-Type, Accept",
          },
        },
      );
    },
    OPTIONS: (req: Request) => {
      return new Response(null, {
        status: 204,
        headers: {
          "Access-Control-Allow-Origin": "https://network-example.app",
          "Access-Control-Allow-Methods": "GET, OPTIONS",
          "Access-Control-Allow-Headers":
            "Origin, X-Requested-With, Content-Type, Accept",
          "Access-Control-Max-Age": "86400",
        },
      });
    },
  },

  /**
   * Rota de health check básica
   */
  "/health": new Response("OK", { status: 200 }),

  /**
   * Echo - retorna todas as informações da requisição
   */
  "/echo": {
    GET: (req: Request) => {
      return Response.json(
        {
          method: req.method,
          url: req.url,
          headers: Object.fromEntries(req.headers.entries()),
          cookies: parseCookies(req.headers.get("Cookie")),
        },
        { headers: getCorsHeaders(req) },
      );
    },
    POST: async (req: Request) => {
      const contentType = req.headers.get("Content-Type") || "";
      let body: any = null;

      if (contentType.includes("application/json")) {
        body = await req.json();
      } else if (contentType.includes("text/")) {
        body = await req.text();
      } else {
        body = await req.arrayBuffer();
      }

      return Response.json(
        {
          method: req.method,
          url: req.url,
          headers: Object.fromEntries(req.headers.entries()),
          cookies: parseCookies(req.headers.get("Cookie")),
          body:
            contentType.includes("application/json") ||
            contentType.includes("text/")
              ? body
              : `<binary data: ${body.byteLength} bytes>`,
        },
        { headers: getCorsHeaders(req) },
      );
    },
  },

  /**
   * Teste de todos os métodos HTTP
   */
  "/methods/get": {
    GET: (req: Request) =>
      Response.json(
        { method: "GET", success: true },
        { headers: getCorsHeaders(req) },
      ),
  },
  "/methods/post": {
    POST: async (req: Request) => {
      const body = await req.json();
      return Response.json(
        { method: "POST", success: true, received: body },
        { headers: getCorsHeaders(req) },
      );
    },
  },
  "/methods/put": {
    PUT: async (req: Request) => {
      const body = await req.json();
      return Response.json(
        { method: "PUT", success: true, received: body },
        { headers: getCorsHeaders(req) },
      );
    },
  },
  "/methods/delete": {
    DELETE: (req: Request) =>
      Response.json(
        { method: "DELETE", success: true },
        { headers: getCorsHeaders(req) },
      ),
  },
  "/methods/patch": {
    PATCH: async (req: Request) => {
      const body = await req.json();
      return Response.json(
        { method: "PATCH", success: true, received: body },
        { headers: getCorsHeaders(req) },
      );
    },
  },
  "/methods/head": {
    HEAD: (req: Request) =>
      new Response(null, {
        status: 200,
        headers: {
          ...getCorsHeaders(req),
          "Content-Length": "42",
          "Content-Type": "application/json",
        },
      }),
  },
  "/methods/options": {
    OPTIONS: (req: Request) =>
      new Response(null, {
        status: 204,
        headers: getCorsHeaders(req),
      }),
  },

  /**
   * Delay - adiciona delay artificial
   * Query params: ?ms=1000
   */
  "/delay": async (req: Request) => {
    const url = new URL(req.url);
    const ms = parseInt(url.searchParams.get("ms") || "1000");
    await delay(ms);
    return Response.json(
      { delayed: ms, timestamp: Date.now() },
      { headers: getCorsHeaders(req) },
    );
  },

  /**
   * Status codes - retorna código de status específico
   * Rota: /status/:code
   */
  "/status/200": new Response("OK", { status: 200 }),
  "/status/201": new Response("Created", { status: 201 }),
  "/status/204": new Response(null, { status: 204 }),
  "/status/400": new Response("Bad Request", { status: 400 }),
  "/status/401": new Response("Unauthorized", { status: 401 }),
  "/status/403": new Response("Forbidden", { status: 403 }),
  "/status/404": new Response("Not Found", { status: 404 }),
  "/status/500": new Response("Internal Server Error", { status: 500 }),
  "/status/502": new Response("Bad Gateway", { status: 502 }),
  "/status/503": new Response("Service Unavailable", { status: 503 }),

  /**
   * Redirects - testa todos os tipos de redirect
   */
  "/redirect/301": (req: Request) =>
    new Response(null, {
      status: 301,
      headers: {
        ...getCorsHeaders(req),
        Location: "/echo",
      },
    }),
  "/redirect/302": (req: Request) =>
    new Response(null, {
      status: 302,
      headers: {
        ...getCorsHeaders(req),
        Location: "/echo",
      },
    }),
  "/redirect/303": (req: Request) =>
    new Response(null, {
      status: 303,
      headers: {
        ...getCorsHeaders(req),
        Location: "/echo",
      },
    }),
  "/redirect/307": (req: Request) =>
    new Response(null, {
      status: 307,
      headers: {
        ...getCorsHeaders(req),
        Location: "/echo",
      },
    }),
  "/redirect/308": (req: Request) =>
    new Response(null, {
      status: 308,
      headers: {
        ...getCorsHeaders(req),
        Location: "/echo",
      },
    }),

  /**
   * Redirect com query params customizável
   * Query: ?to=/target&code=302
   */
  "/redirect": (req: Request) => {
    const url = new URL(req.url);
    const to = url.searchParams.get("to") || "/echo";
    const code = parseInt(url.searchParams.get("code") || "302");

    return new Response(null, {
      status: code,
      headers: {
        ...getCorsHeaders(req),
        Location: to,
      },
    });
  },

  /**
   * Redirect em cadeia - testa múltiplos redirects
   * Query: ?count=5
   */
  "/redirect/chain": (req: Request) => {
    const url = new URL(req.url);
    const count = parseInt(url.searchParams.get("count") || "1");

    if (count <= 1) {
      return Response.json(
        { redirects: "completed" },
        { headers: getCorsHeaders(req) },
      );
    }

    return new Response(null, {
      status: 302,
      headers: {
        ...getCorsHeaders(req),
        Location: `/redirect/chain?count=${count - 1}`,
      },
    });
  },

  "/redirect/cookies/set-and-forward": (req: Request) => {
    const url = new URL(req.url);
    const name = url.searchParams.get("name") || "redirect_cookie";
    const value = url.searchParams.get("value") || "redirect_value";
    const to = url.searchParams.get("to") || "/cookies/get";
    const code = parseInt(url.searchParams.get("code") || "302");
    const options = getCookieOptionsFromUrl(url);
    const setCookie = createSetCookie(name, value, options);

    return new Response(null, {
      status: code,
      headers: {
        ...getCorsHeaders(req),
        Location: to,
        "Set-Cookie": setCookie,
      },
    });
  },

  /**
   * Cookies - testes completos de cookies
   */
  "/cookies/set": (req: Request) => {
    const url = new URL(req.url);
    const name = url.searchParams.get("name") || "test_cookie";
    const value = url.searchParams.get("value") || "test_value";
    const options = getCookieOptionsFromUrl(url);
    const setCookie = createSetCookie(name, value, options);

    return Response.json(
      {
        action: "set",
        set: true,
        name,
        value,
        options: serializeCookieOptions(options),
        setCookie,
        query: getQueryParams(url),
      },
      {
        headers: {
          ...getCorsHeaders(req),
          "Set-Cookie": setCookie,
        },
      },
    );
  },

  "/cookies/set-multiple": (req: Request) => {
    const headers = new Headers(getCorsHeaders(req));
    const cookies = [
      {
        name: "cookie1",
        value: "value1",
        options: { path: "/", maxAge: 3600 } satisfies CookieOptions,
      },
      {
        name: "cookie2",
        value: "value2",
        options: { path: "/", httpOnly: true } satisfies CookieOptions,
      },
      {
        name: "cookie3",
        value: "value3",
        options: { path: "/", sameSite: "Lax" } satisfies CookieOptions,
      },
    ];

    const serializedCookies = cookies.map(({ name, value, options }) => {
      const setCookie = createSetCookie(name, value, options);
      headers.append("Set-Cookie", setCookie);

      return {
        name,
        value,
        options: serializeCookieOptions(options),
        setCookie,
      };
    });

    return Response.json(
      {
        action: "set-multiple",
        set: true,
        count: serializedCookies.length,
        cookies: serializedCookies,
      },
      { headers },
    );
  },

  "/cookies/get": (req: Request) => createCookieInspectionResponse(req),
  "/cookies/path-scoped/check": (req: Request) => createCookieInspectionResponse(req),
  "/cookies/path-other/check": (req: Request) => createCookieInspectionResponse(req),

  "/cookies/delete": (req: Request) => {
    const url = new URL(req.url);
    const name = url.searchParams.get("name") || "test_cookie";
    const setCookie = createSetCookie(name, "", {
      path: url.searchParams.get("path") || "/",
      domain: url.searchParams.get("domain") || undefined,
      maxAge: 0,
      expires: new Date(0),
    });

    return Response.json(
      {
        action: "delete",
        deleted: true,
        name,
        setCookie,
      },
      {
        headers: {
          ...getCorsHeaders(req),
          "Set-Cookie": setCookie,
        },
      },
    );
  },

  /**
   * Cache - testes de cache headers
   */
  "/cache/etag": (req: Request) => {
    const content = JSON.stringify({
      data: "test content",
      timestamp: Date.now(),
    });
    const etag = generateETag(content);

    if (checkNotModified(req, etag)) {
      return new Response(null, {
        status: 304,
        headers: {
          ...getCorsHeaders(req),
          ETag: etag,
        },
      });
    }

    return new Response(content, {
      headers: {
        ...getCorsHeaders(req),
        "Content-Type": "application/json",
        ETag: etag,
        "Cache-Control": "public, max-age=3600",
      },
    });
  },

  "/cache/last-modified": (req: Request) => {
    const lastModified = new Date("2026-01-01T00:00:00Z");
    const content = JSON.stringify({ data: "cached content" });
    const etag = generateETag(content);

    if (checkNotModified(req, etag, lastModified)) {
      return new Response(null, {
        status: 304,
        headers: {
          ...getCorsHeaders(req),
          "Last-Modified": lastModified.toUTCString(),
          ETag: etag,
        },
      });
    }

    return new Response(content, {
      headers: {
        ...getCorsHeaders(req),
        "Content-Type": "application/json",
        "Last-Modified": lastModified.toUTCString(),
        ETag: etag,
        "Cache-Control": "public, max-age=3600",
      },
    });
  },

  "/cache/no-cache": (req: Request) =>
    Response.json(
      { timestamp: Date.now() },
      {
        headers: {
          ...getCorsHeaders(req),
          "Cache-Control": "no-cache, no-store, must-revalidate",
          Pragma: "no-cache",
          Expires: "0",
        },
      },
    ),

  "/cache/max-age": (req: Request) => {
    const url = new URL(req.url);
    const maxAge = url.searchParams.get("seconds") || "3600";

    return Response.json(
      { timestamp: Date.now() },
      {
        headers: {
          ...getCorsHeaders(req),
          "Cache-Control": `public, max-age=${maxAge}`,
        },
      },
    );
  },

  /**
   * Headers customizados
   */
  "/headers/custom": (req: Request) => {
    return new Response("OK", {
      headers: {
        ...getCorsHeaders(req),
        "X-Custom-Header": "CustomValue",
        "X-Request-ID": crypto.randomUUID(),
        "X-Timestamp": Date.now().toString(),
      },
    });
  },

  "/headers/echo": (req: Request) => {
    return Response.json(
      {
        received: Object.fromEntries(req.headers.entries()),
      },
      { headers: getCorsHeaders(req) },
    );
  },

  /**
   * Content Types - diferentes tipos de conteúdo
   */
  "/content/json": (req: Request) =>
    Response.json(
      { message: "JSON response", data: [1, 2, 3] },
      { headers: getCorsHeaders(req) },
    ),

  "/content/text": (req: Request) =>
    new Response("Plain text response", {
      headers: {
        ...getCorsHeaders(req),
        "Content-Type": "text/plain",
      },
    }),

  "/content/html": (req: Request) =>
    new Response("<html><body><h1>HTML Response</h1></body></html>", {
      headers: {
        ...getCorsHeaders(req),
        "Content-Type": "text/html",
      },
    }),

  "/content/binary": (req: Request) => {
    const size = parseInt(new URL(req.url).searchParams.get("size") || "1024");
    const data = generateBinaryData(size);

    return new Response(data, {
      headers: {
        ...getCorsHeaders(req),
        "Content-Type": "application/octet-stream",
        "Content-Length": size.toString(),
      },
    });
  },

  /**
   * Authentication
   */
  "/auth/basic": (req: Request) => {
    const authHeader = req.headers.get("Authorization");

    if (!authHeader || !authHeader.startsWith("Basic ")) {
      return new Response("Unauthorized", {
        status: 401,
        headers: {
          ...getCorsHeaders(req),
          "WWW-Authenticate": 'Basic realm="Test Realm"',
        },
      });
    }

    const credentials = atob(authHeader.slice(6));
    const [username, password] = credentials.split(":");

    if (username === "test" && password === "password") {
      return Response.json(
        { authenticated: true, username },
        { headers: getCorsHeaders(req) },
      );
    }

    return new Response("Unauthorized", {
      status: 401,
      headers: {
        ...getCorsHeaders(req),
        "WWW-Authenticate": 'Basic realm="Test Realm"',
      },
    });
  },

  "/auth/bearer": (req: Request) => {
    const authHeader = req.headers.get("Authorization");

    if (!authHeader || !authHeader.startsWith("Bearer ")) {
      return new Response("Unauthorized", {
        status: 401,
        headers: {
          ...getCorsHeaders(req),
          "WWW-Authenticate": 'Bearer realm="Test Realm"',
        },
      });
    }

    const token = authHeader.slice(7);

    if (token === "valid-token-123") {
      return Response.json(
        { authenticated: true, token },
        { headers: getCorsHeaders(req) },
      );
    }

    return new Response("Unauthorized", {
      status: 401,
      headers: getCorsHeaders(req),
    });
  },

  /**
   * Compression - teste de diferentes algoritmos
   * Nota: Bun comprime automaticamente se Accept-Encoding está presente
   */
  "/compression/auto": (req: Request) => {
    const largeData = {
      message: "This is a large response that should be compressed".repeat(100),
      data: Array.from({ length: 1000 }, (_, i) => ({
        id: i,
        value: `item-${i}`,
      })),
    };

    return Response.json(largeData, {
      headers: getCorsHeaders(req),
    });
  },

  /**
   * Range Requests - suporte a partial content
   */
  "/range": (req: Request) => {
    const content = "0123456789".repeat(100); // 1000 bytes
    const rangeHeader = req.headers.get("Range");

    if (!rangeHeader) {
      return new Response(content, {
        headers: {
          ...getCorsHeaders(req),
          "Content-Type": "text/plain",
          "Content-Length": content.length.toString(),
          "Accept-Ranges": "bytes",
        },
      });
    }

    // Parse range header (simplificado)
    const match = rangeHeader.match(/bytes=(\d+)-(\d*)/);
    if (!match) {
      return new Response("Invalid range", { status: 416 });
    }

    const start = parseInt(match[1]);
    const end = match[2] ? parseInt(match[2]) : content.length - 1;
    const partialContent = content.slice(start, end + 1);

    return new Response(partialContent, {
      status: 206,
      headers: {
        ...getCorsHeaders(req),
        "Content-Type": "text/plain",
        "Content-Range": `bytes ${start}-${end}/${content.length}`,
        "Content-Length": partialContent.length.toString(),
        "Accept-Ranges": "bytes",
      },
    });
  },

  /**
   * Timeout - rota que nunca responde (para testes de timeout)
   */
  "/timeout/never": async (req: Request) => {
    await delay(300000); // 5 minutos
    return new Response("Should have timed out");
  },

  /**
   * Large payload - teste com payloads grandes
   */
  "/large/download": (req: Request) => {
    const url = new URL(req.url);
    const sizeMB = parseInt(url.searchParams.get("mb") || "1");
    const sizeBytes = sizeMB * 1024 * 1024;
    const data = generateBinaryData(sizeBytes);

    return new Response(data, {
      headers: {
        ...getCorsHeaders(req),
        "Content-Type": "application/octet-stream",
        "Content-Length": sizeBytes.toString(),
        "Content-Disposition": `attachment; filename="test-${sizeMB}mb.bin"`,
      },
    });
  },

  "/large/upload": {
    POST: async (req: Request) => {
      const data = await req.arrayBuffer();
      return Response.json(
        {
          received: data.byteLength,
          mb: (data.byteLength / 1024 / 1024).toFixed(2),
        },
        { headers: getCorsHeaders(req) },
      );
    },
  },

  /**
   * Upload FormData - processa multipart/form-data (RFC 7578)
   */
  "/upload/form": {
    POST: async (req: Request) => {
      try {
        const formData = await req.formData();
        const fields: Record<string, any> = {};
        const files: Array<{ name: string; filename: string; size: number; type: string }> = [];

        for (const [key, value] of formData.entries()) {
          if (typeof value !== "string") {
            // It's a File/Blob
            const file = value as unknown as { name: string; size: number; type: string };
            files.push({
              name: key,
              filename: file.name,
              size: file.size,
              type: file.type,
            });
          } else {
            // Handle multiple values for same field
            if (fields[key]) {
              if (Array.isArray(fields[key])) {
                fields[key].push(value);
              } else {
                fields[key] = [fields[key], value];
              }
            } else {
              fields[key] = value;
            }
          }
        }

        return Response.json(
          {
            success: true,
            fields,
            files,
            totalFields: Object.keys(fields).length,
            totalFiles: files.length,
          },
          { headers: getCorsHeaders(req) },
        );
      } catch (error) {
        return Response.json(
          {
            success: false,
            error: error instanceof Error ? error.message : "Failed to parse FormData",
          },
          {
            status: 400,
            headers: getCorsHeaders(req),
          },
        );
      }
    },
  },

  /**
   * Status 413 - Payload Too Large
   */
  "/status/413": new Response("Payload Too Large", { status: 413 }),
};
