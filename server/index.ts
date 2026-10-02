/**
 * @file index.ts
 * @description Servidor de testes Bun para biblioteca de rede React Native
 *
 * Este servidor implementa 100% dos recursos HTTP/HTTPS/WebSocket/SSE/WebStreams
 * necessários para testar completamente a biblioteca de rede C++.
 *
 * Estrutura:
 * - HTTP/HTTPS: Todos os métodos, headers, cookies, cache, redirects, etc
 * - WebSocket: Echo, broadcast, rooms, binary, ping/pong, stress tests
 * - SSE: Eventos simples, nomeados, multiline, progress, infinite
 * - WebStreams: Chunks, backpressure, binary, JSON, cancel, error
 *
 * Versão do Bun: 1.3.8+
 * Data: 2026-02-15
 */

import { httpRoutes } from "./routes/http";
import { sseRoutes } from "./routes/sse";
import { streamRoutes } from "./routes/streams";
import { webSocketHandlers, createWebSocketData, type WebSocketData } from "./routes/websocket";
import { getCorsHeaders, isPreflight, createPreflightResponse } from "./utils/cors";
import { createSetCookie } from "./utils/helpers";

/**
 * Porta do servidor
 */
const PORT = parseInt(process.env.PORT || "3000");
const HOST = process.env.HOST || "0.0.0.0";

/**
 * Mapa consolidado de todas as rotas
 */
const allRoutes = {
  ...httpRoutes,
  ...sseRoutes,
  ...streamRoutes,
};

function parseCookieFlag(value: string | null): boolean {
  if (value === null) return false;

  const normalized = value.trim().toLowerCase();
  if (normalized === "" || normalized === "1" || normalized === "true" || normalized === "yes" || normalized === "on") {
    return true;
  }

  return false;
}

function parseCookieSameSite(value: string | null): "Strict" | "Lax" | "None" | undefined {
  if (value === null) return undefined;

  const normalized = value.trim().toLowerCase();
  if (normalized === "strict") return "Strict";
  if (normalized === "lax") return "Lax";
  if (normalized === "none") return "None";

  return undefined;
}

function buildHandshakeSetCookie(url: URL, suffix = ""): string | null {
  const searchParams = url.searchParams;
  const name = searchParams.get(`setCookieName${suffix}`);
  const value = searchParams.get(`setCookieValue${suffix}`);

  if (!name || value === null) {
    return null;
  }

  return createSetCookie(name, value, {
    path: "/",
    secure: parseCookieFlag(searchParams.get(`setCookieSecure${suffix}`)),
    httpOnly: parseCookieFlag(searchParams.get(`setCookieHttpOnly${suffix}`)),
    sameSite: parseCookieSameSite(searchParams.get(`setCookieSameSite${suffix}`)),
  });
}

/**
 * Handler principal do servidor
 */
const server = Bun.serve<WebSocketData>({
  port: PORT,
  hostname: HOST,

  /**
   * Configuração TLS para HTTPS (opcional)
   * Descomente e configure os certificados se necessário
   */
  // tls: {
  //   cert: Bun.file("./certs/cert.pem"),
  //   key: Bun.file("./certs/key.pem"),
  // },

  /**
   * Handler de requisições HTTP
   */
  async fetch(req: Request, server: any): Promise<Response | undefined> {
    const url = new URL(req.url);
    const path = url.pathname;

    // Log de requisições
    console.log(`[${new Date().toISOString()}] ${req.method} ${path}`);

    // Handle WebSocket upgrade
    for (const wsPath in webSocketHandlers) {
      if (path === wsPath) {
        const upgradeOptions: { data: WebSocketData; headers?: Headers } = {
          data: createWebSocketData(req),
        };

        if (path === "/ws/echo") {
          const headers = new Headers();
          const firstCookie = buildHandshakeSetCookie(url);
          const secondCookie = buildHandshakeSetCookie(url, "2");

          if (firstCookie) {
            headers.append("Set-Cookie", firstCookie);
          }

          if (secondCookie) {
            headers.append("Set-Cookie", secondCookie);
          }

          if (headers.has("Set-Cookie")) {
            upgradeOptions.headers = headers;
          }
        }

        const upgraded = server.upgrade(req, upgradeOptions);

        if (upgraded) {
          return undefined; // Retorna undefined quando upgrade é bem-sucedido
        }

        return new Response("WebSocket upgrade failed", {
          status: 500,
          headers: getCorsHeaders(req),
        });
      }
    }

    // Handle CORS preflight
    if (isPreflight(req)) {
      return createPreflightResponse(req);
    }

    // Match route in allRoutes
    const route = allRoutes[path as keyof typeof allRoutes];

    if (route) {
      try {
        // Se a rota é um objeto com métodos HTTP
        if (typeof route === "object" && !("then" in route) && !(route instanceof Response)) {
          const methodHandler = (route as any)[req.method];

          if (methodHandler) {
            const response = await methodHandler(req, server);
            return response;
          }

          // Método não suportado
          return new Response("Method Not Allowed", {
            status: 405,
            headers: {
              ...getCorsHeaders(req),
              Allow: Object.keys(route).join(", "),
            },
          });
        }

        // Se a rota é uma Response direta
        if (route instanceof Response) {
          return route;
        }

        // Se a rota é uma função
        if (typeof route === "function") {
          const response = await route(req);
          return response;
        }
      } catch (error) {
        console.error(`Error handling ${path}:`, error);
        return new Response(
          JSON.stringify({
            error: "Internal Server Error",
            message: error instanceof Error ? error.message : String(error),
          }),
          {
            status: 500,
            headers: {
              ...getCorsHeaders(req),
              "Content-Type": "application/json",
            },
          },
        );
      }
    }

    // Wildcard routes
    if (path.startsWith("/api/")) {
      return new Response(
        JSON.stringify({
          error: "Not Found",
          path,
          availableRoutes: Object.keys(allRoutes),
        }),
        {
          status: 404,
          headers: {
            ...getCorsHeaders(req),
            "Content-Type": "application/json",
          },
        },
      );
    }

    // Root - retorna informações sobre o servidor
    if (path === "/" || path === "") {
      return new Response(
        JSON.stringify(
          {
            name: "Browser Network Test Server",
            version: "1.0.0",
            bun: Bun.version,
            timestamp: new Date().toISOString(),
            routes: {
              http: Object.keys(httpRoutes),
              sse: Object.keys(sseRoutes),
              streams: Object.keys(streamRoutes),
              websocket: Object.keys(webSocketHandlers),
            },
            endpoints: {
              health: "/health",
              echo: "/echo",
              documentation: "https://github.com/your-repo/browser-network",
            },
          },
          null,
          2,
        ),
        {
          headers: {
            ...getCorsHeaders(req),
            "Content-Type": "application/json",
          },
        },
      );
    }

    // 404
    return new Response(
      JSON.stringify({
        error: "Not Found",
        path,
        message: "Route not found",
        availableRoutes: {
          http: Object.keys(httpRoutes).slice(0, 10),
          sse: Object.keys(sseRoutes),
          streams: Object.keys(streamRoutes),
          websocket: Object.keys(webSocketHandlers),
        },
      }),
      {
        status: 404,
        headers: {
          ...getCorsHeaders(req),
          "Content-Type": "application/json",
        },
      },
    );
  },

  /**
   * Handlers WebSocket
   * Determina qual handler usar baseado no path da conexão
   */
  websocket: {
    // Dados tipados para o WebSocket
    // data: {} as WebSocketData, // Já está tipado no serve<WebSocketData>

    open(ws) {
      const path = (ws as any).data.path || "/ws/echo";
      const handler = webSocketHandlers[path];

      if (handler && handler.open) {
        handler.open(ws);
      }
    },

    message(ws, message) {
      const path = (ws as any).data.path || "/ws/echo";
      const handler = webSocketHandlers[path];

      if (handler && handler.message) {
        handler.message(ws, message);
      }
    },

    close(ws, code: number, reason: string) {
      const path = (ws as any).data.path || "/ws/echo";
      const handler = webSocketHandlers[path];

      if (handler && handler.close) {
        handler.close(ws, code, reason);
      }
    },

    drain(ws) {
      const path = (ws as any).data.path || "/ws/echo";
      const handler = webSocketHandlers[path];

      if (handler && handler.drain) {
        handler.drain(ws);
      }
    },

    // Configurações globais de WebSocket
    maxPayloadLength: 16 * 1024 * 1024, // 16 MB
    idleTimeout: 120, // 120 segundos
    backpressureLimit: 1024 * 1024, // 1 MB
    closeOnBackpressureLimit: false,
    sendPings: true,
    publishToSelf: false,

    // Compression (per-message deflate)
    perMessageDeflate: true,
  },

  /**
   * Error handler
   */
  error(error: Error): Response {
    console.error("Server error:", error);

    return new Response(
      JSON.stringify({
        error: "Internal Server Error",
        message: error.message,
        stack: process.env.NODE_ENV === "development" ? error.stack : undefined,
      }),
      {
        status: 500,
        headers: {
          "Content-Type": "application/json",
          "Access-Control-Allow-Origin": "*",
        },
      },
    );
  },
});

// Fix para determinar o path correto do WebSocket
// Modifica o upgrade para incluir o path nos dados
const originalUpgrade = server.upgrade.bind(server);
server.upgrade = function (req: Request, options?: any) {
  const url = new URL(req.url);
  const enhancedOptions = {
    ...options,
    data: {
      ...options?.data,
      path: url.pathname,
    },
  };
  return originalUpgrade(req, enhancedOptions);
};

/**
 * Log de inicialização
 */
console.log("\n" + "=".repeat(60));
console.log("🚀 Browser Network Test Server");
console.log("=".repeat(60));
console.log(`📍 URL: http://${HOST}:${PORT}`);
console.log(`🔧 Bun version: ${Bun.version}`);
console.log(`📅 Started at: ${new Date().toISOString()}`);
console.log("\n📋 Available endpoints:");
console.log(`   HTTP Routes: ${Object.keys(httpRoutes).length}`);
console.log(`   SSE Routes: ${Object.keys(sseRoutes).length}`);
console.log(`   Stream Routes: ${Object.keys(streamRoutes).length}`);
console.log(`   WebSocket Routes: ${Object.keys(webSocketHandlers).length}`);
console.log("\n✅ Server ready to accept connections");
console.log("=".repeat(60) + "\n");

/**
 * Graceful shutdown
 */
process.on("SIGINT", async () => {
  console.log("\n\n🛑 Shutting down server...");
  await server.stop();
  console.log("✅ Server stopped");
  process.exit(0);
});

process.on("SIGTERM", async () => {
  console.log("\n\n🛑 Shutting down server...");
  await server.stop();
  console.log("✅ Server stopped");
  process.exit(0);
});
