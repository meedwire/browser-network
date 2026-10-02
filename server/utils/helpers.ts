/**
 * @file helpers.ts
 * @description Funções auxiliares para o servidor de testes
 */

/**
 * Adiciona delay artificial para testes de timeout
 */
export function delay(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

/**
 * Gera dados binários aleatórios para testes
 */
export function generateBinaryData(size: number): Uint8Array {
  const buffer = new Uint8Array(size);
  for (let i = 0; i < size; i++) {
    buffer[i] = Math.floor(Math.random() * 256);
  }
  return buffer;
}

/**
 * Converte objeto em formato JSON com indentação
 */
export function prettyJson(obj: any): string {
  return JSON.stringify(obj, null, 2);
}

/**
 * Extrai parâmetros da query string
 */
export function getQueryParams(url: URL): Record<string, string> {
  const params: Record<string, string> = {};
  url.searchParams.forEach((value, key) => {
    params[key] = value;
  });
  return params;
}

/**
 * Parseia cookies da requisição
 */
export function parseCookies(cookieHeader: string | null): Record<string, string> {
  if (!cookieHeader) return {};

  const cookies: Record<string, string> = {};
  cookieHeader.split(";").forEach((cookie) => {
    const trimmedCookie = cookie.trim();
    const separatorIndex = trimmedCookie.indexOf("=");

    if (separatorIndex > 0) {
      const key = trimmedCookie.slice(0, separatorIndex);
      const value = trimmedCookie.slice(separatorIndex + 1);
      cookies[key] = decodeURIComponent(value);
    }
  });
  return cookies;
}

/**
 * Cria um header Set-Cookie completo
 */
export interface CookieOptions {
  path?: string;
  domain?: string;
  maxAge?: number;
  expires?: Date;
  httpOnly?: boolean;
  secure?: boolean;
  sameSite?: "Strict" | "Lax" | "None";
}

export function createSetCookie(
  name: string,
  value: string,
  options: CookieOptions = {},
): string {
  let cookie = `${name}=${encodeURIComponent(value)}`;

  if (options.path) cookie += `; Path=${options.path}`;
  if (options.domain) cookie += `; Domain=${options.domain}`;
  if (options.maxAge !== undefined) cookie += `; Max-Age=${options.maxAge}`;
  if (options.expires) cookie += `; Expires=${options.expires.toUTCString()}`;
  if (options.httpOnly) cookie += "; HttpOnly";
  if (options.secure) cookie += "; Secure";
  if (options.sameSite) cookie += `; SameSite=${options.sameSite}`;

  return cookie;
}

/**
 * Gera um ETag simples baseado no conteúdo
 */
export function generateETag(content: string): string {
  // Hash simples para ETag (em produção, usar crypto.subtle)
  let hash = 0;
  for (let i = 0; i < content.length; i++) {
    const char = content.charCodeAt(i);
    hash = (hash << 5) - hash + char;
    hash = hash & hash; // Convert to 32bit integer
  }
  return `"${Math.abs(hash).toString(16)}"`;
}

/**
 * Verifica se o conteúdo foi modificado baseado em headers condicionais
 */
export function checkNotModified(
  request: Request,
  etag: string,
  lastModified?: Date,
): boolean {
  const ifNoneMatch = request.headers.get("If-None-Match");
  const ifModifiedSince = request.headers.get("If-Modified-Since");

  // Verifica ETag
  if (ifNoneMatch && ifNoneMatch === etag) {
    return true;
  }

  // Verifica Last-Modified
  if (ifModifiedSince && lastModified) {
    const modifiedSince = new Date(ifModifiedSince);
    if (lastModified <= modifiedSince) {
      return true;
    }
  }

  return false;
}
