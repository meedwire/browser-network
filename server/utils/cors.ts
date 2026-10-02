/**
 * @file cors.ts
 * @description Utilitários para configuração de CORS seguindo as especificações do navegador
 *
 * Implementa headers CORS completos conforme a especificação W3C:
 * - Access-Control-Allow-Origin
 * - Access-Control-Allow-Methods
 * - Access-Control-Allow-Headers
 * - Access-Control-Allow-Credentials
 * - Access-Control-Max-Age
 * - Access-Control-Expose-Headers
 */

export interface CorsOptions {
  allowOrigin?: string | string[] | ((origin: string) => boolean);
  allowMethods?: string[];
  allowHeaders?: string[];
  exposeHeaders?: string[];
  allowCredentials?: boolean;
  maxAge?: number;
}

/**
 * Gera headers CORS baseado na origem da requisição
 */
export function getCorsHeaders(request: Request, options?: CorsOptions): HeadersInit {
  const origin = request.headers.get("Origin") || "*";
  const requestHeaders = request.headers.get("Access-Control-Request-Headers");

  const defaultOptions: CorsOptions = {
    allowOrigin: "*",
    allowMethods: ["GET", "POST", "PUT", "DELETE", "PATCH", "OPTIONS", "HEAD"],
    allowHeaders: requestHeaders ? requestHeaders.split(",").map((h) => h.trim()) : ["*"],
    exposeHeaders: ["Content-Length", "Content-Type", "X-Cache"],
    allowCredentials: true,
    maxAge: 86400, // 24 horas
    ...options,
  };

  // Determinar origem permitida
  let allowOrigin = "*";
  if (typeof defaultOptions.allowOrigin === "string") {
    allowOrigin = defaultOptions.allowOrigin;
  } else if (Array.isArray(defaultOptions.allowOrigin)) {
    allowOrigin = defaultOptions.allowOrigin.includes(origin) ? origin : "";
  } else if (typeof defaultOptions.allowOrigin === "function") {
    allowOrigin = defaultOptions.allowOrigin(origin) ? origin : "";
  }

  const headers: Record<string, string> = {
    "Access-Control-Allow-Origin": allowOrigin || origin,
    "Access-Control-Allow-Methods": defaultOptions.allowMethods!.join(", "),
    "Access-Control-Allow-Headers": defaultOptions.allowHeaders!.join(", "),
    "Access-Control-Max-Age": defaultOptions.maxAge!.toString(),
  };

  if (defaultOptions.exposeHeaders && defaultOptions.exposeHeaders.length > 0) {
    headers["Access-Control-Expose-Headers"] = defaultOptions.exposeHeaders.join(", ");
  }

  if (defaultOptions.allowCredentials) {
    headers["Access-Control-Allow-Credentials"] = "true";
  }

  return headers;
}

/**
 * Verifica se a requisição é um preflight (OPTIONS)
 */
export function isPreflight(request: Request): boolean {
  return (
    request.method === "OPTIONS" &&
    request.headers.has("Access-Control-Request-Method")
  );
}

/**
 * Cria uma resposta de preflight
 */
export function createPreflightResponse(request: Request, options?: CorsOptions): Response {
  return new Response(null, {
    status: 204,
    headers: getCorsHeaders(request, options),
  });
}
