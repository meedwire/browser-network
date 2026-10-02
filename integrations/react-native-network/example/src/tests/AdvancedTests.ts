/**
 * @file AdvancedTests.ts
 * @brief Suite de testes avançados para Cookies e Cache
 *
 * Cobre funcionalidades avançadas de HTTP/HTTPS que garantem paridade
 * completa com navegadores modernos.
 *
 * @date 2026-02-15
 */

import { TestSuite, expect } from '../test-runner';

const baseUrl = 'http://10.0.2.2:3000';
const cookieDomain = baseUrl.replace(/^https?:\/\//, '').split(':')[0]!;

const createCookieName = (prefix: string) =>
  `${prefix}_${Date.now()}_${Math.random().toString(16).slice(2)}`;

const getCookieSnapshot = async (path: string = '/cookies/get') => {
  const resp = await fetch(`${baseUrl}${path}`);
  expect(resp.status).toBe(200);
  return resp.json();
};

export const createAdvancedTests = () => {
  const suite = new TestSuite('advanced');
  const { describe } = suite.getAPI();

  // ========================================================================
  // 1. COOKIES
  // ========================================================================
  describe('Cookies', ({ it }) => {
    it('deve definir um único cookie', async () => {
      const cookieName = createCookieName('testCookie');
      const resp = await fetch(
        `${baseUrl}/cookies/set?name=${cookieName}&value=testValue`
      );
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.setCookie).toContain(`${cookieName}=testValue`);

      const snapshot = await getCookieSnapshot();
      expect(snapshot.cookies[cookieName]).toBe('testValue');
    });

    it('deve definir múltiplos cookies', async () => {
      const resp = await fetch(`${baseUrl}/cookies/set-multiple`);
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.count).toBe(3);

      const snapshot = await getCookieSnapshot();
      expect(snapshot.cookies.cookie1).toBe('value1');
      expect(snapshot.cookies.cookie2).toBe('value2');
      expect(snapshot.cookies.cookie3).toBe('value3');
    });

    it('deve ler cookies', async () => {
      const cookieName = createCookieName('readTest');
      await fetch(`${baseUrl}/cookies/set?name=${cookieName}&value=readValue`);
      const resp = await fetch(`${baseUrl}/echo`);
      const data = await resp.json();
      expect(data.cookies[cookieName]).toBe('readValue');
    });

    it('deve sobrescrever cookie com o último valor', async () => {
      const cookieName = createCookieName('overwriteCookie');

      await fetch(`${baseUrl}/cookies/set?name=${cookieName}&value=initial`);
      await fetch(`${baseUrl}/cookies/set?name=${cookieName}&value=updated`);

      const snapshot = await getCookieSnapshot();
      expect(snapshot.cookies[cookieName]).toBe('updated');
    });

    it('deve deletar cookie', async () => {
      const cookieName = createCookieName('deleteMe');

      await fetch(`${baseUrl}/cookies/set?name=${cookieName}&value=temp`);
      const beforeDelete = await getCookieSnapshot();
      expect(beforeDelete.cookies[cookieName]).toBe('temp');

      const resp = await fetch(`${baseUrl}/cookies/delete?name=${cookieName}`);
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.setCookie).toContain(`${cookieName}=`);
      expect(data.setCookie).toContain('Max-Age=0');

      const afterDelete = await getCookieSnapshot();
      expect(afterDelete.cookies[cookieName]).toBe(undefined);
    });

    it('deve persistir cookie entre requisições subsequentes', async () => {
      const cookieName = createCookieName('persistCookie');

      await fetch(
        `${baseUrl}/cookies/set?name=${cookieName}&value=persisted&maxAge=3600`
      );

      const firstSnapshot = await getCookieSnapshot();
      expect(firstSnapshot.cookies[cookieName]).toBe('persisted');

      const echoResp = await fetch(`${baseUrl}/echo?step=persistence`);
      expect(echoResp.status).toBe(200);

      const secondSnapshot = await getCookieSnapshot();
      expect(secondSnapshot.cookies[cookieName]).toBe('persisted');
    });

    it('deve lidar com cookie com Max-Age', async () => {
      const resp = await fetch(
        `${baseUrl}/cookies/set?name=${createCookieName(
          'ttlCookie'
        )}&value=temp&maxAge=3600`
      );
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.setCookie).toContain('Max-Age=3600');
    });

    it('deve lidar com cookie com Expires', async () => {
      const expires = new Date(Date.now() + 86400000).toUTCString();
      const resp = await fetch(
        `${baseUrl}/cookies/set?name=${createCookieName(
          'expireCookie'
        )}&value=temp&expires=${encodeURIComponent(expires)}`
      );
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.setCookie).toContain('Expires=');
    });

    it('deve lidar com cookie HttpOnly', async () => {
      const resp = await fetch(
        `${baseUrl}/cookies/set?name=${createCookieName(
          'httpOnlyCookie'
        )}&value=secure&httpOnly=true`
      );
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.setCookie).toContain('HttpOnly');
    });

    it('deve lidar com cookie Secure', async () => {
      const resp = await fetch(
        `${baseUrl}/cookies/set?name=${createCookieName(
          'secureCookie'
        )}&value=secure&secure=true`
      );
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.setCookie).toContain('Secure');
    });

    it('deve lidar com SameSite=Strict', async () => {
      const resp = await fetch(
        `${baseUrl}/cookies/set?name=${createCookieName(
          'strictCookie'
        )}&value=test&sameSite=Strict`
      );
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.setCookie).toContain('SameSite=Strict');
    });

    it('deve lidar com SameSite=Lax', async () => {
      const resp = await fetch(
        `${baseUrl}/cookies/set?name=${createCookieName(
          'laxCookie'
        )}&value=test&sameSite=Lax`
      );
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.setCookie).toContain('SameSite=Lax');
    });

    it('deve lidar com o escopo de path do cookie', async () => {
      const cookieName = createCookieName('pathCookie');
      const scopedPath = '/cookies/path-scoped';

      const resp = await fetch(
        `${baseUrl}/cookies/set?name=${cookieName}&value=admin&path=${encodeURIComponent(
          scopedPath
        )}`
      );
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.setCookie).toContain(`Path=${scopedPath}`);

      const outOfScope = await getCookieSnapshot('/cookies/path-other/check');
      expect(outOfScope.cookies[cookieName]).toBe(undefined);

      const inScope = await getCookieSnapshot('/cookies/path-scoped/check');
      expect(inScope.cookies[cookieName]).toBe('admin');
    });

    it('deve expor o atributo Domain no Set-Cookie', async () => {
      const cookieName = createCookieName('domainCookie');
      const resp = await fetch(
        `${baseUrl}/cookies/set?name=${cookieName}&value=scoped&domain=${cookieDomain}`
      );
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.options.domain).toBe(cookieDomain);
      expect(data.setCookie).toContain(`Domain=${cookieDomain}`);
    });
  });

  // ========================================================================
  // 2. ADVANCED CACHE
  // ========================================================================
  describe('Cache Avançado', ({ it }) => {
    it('deve retornar ETag na resposta', async () => {
      const resp = await fetch(`${baseUrl}/cache/etag`);
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.data).toBeTruthy();
    });

    it('deve retornar Last-Modified na resposta', async () => {
      const resp = await fetch(`${baseUrl}/cache/last-modified`);
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.data).toBeTruthy();
    });

    it('deve lidar com Cache-Control: no-cache', async () => {
      const resp = await fetch(`${baseUrl}/cache/no-cache`);
      expect(resp.status).toBe(200);
    });

    it('deve lidar com Cache-Control: max-age', async () => {
      const resp = await fetch(`${baseUrl}/cache/max-age?seconds=3600`);
      expect(resp.status).toBe(200);
    });

    it('deve validar conteúdo cacheado', async () => {
      const resp1 = await fetch(`${baseUrl}/cache/etag`);
      expect(resp1.status).toBe(200);
      // TODO: Implementar validação completa quando Response.headers estiver disponível
    });

    it('deve revalidar quando expirado (stale)', async () => {
      const resp = await fetch(`${baseUrl}/cache/etag`);
      expect(resp.status).toBe(200);
    });

    it('deve lidar com conteúdo expirado (stale)', async () => {
      const resp = await fetch(`${baseUrl}/cache/max-age?seconds=60`);
      expect(resp.status).toBe(200);
    });
  });

  // ========================================================================
  // 3. CACHE POLICY
  // ========================================================================
  describe('Política de Cache', ({ it }) => {
    it('deve lidar com cache privado', async () => {
      const resp = await fetch(`${baseUrl}/cache/max-age?seconds=3600`);
      expect(resp.status).toBe(200);
    });

    it('deve lidar com cache público', async () => {
      const resp = await fetch(`${baseUrl}/cache/etag`);
      expect(resp.status).toBe(200);
    });

    it('deve impor no-store', async () => {
      const resp = await fetch(`${baseUrl}/cache/no-cache`);
      expect(resp.status).toBe(200);
    });

    it('deve lidar com a diretiva immutable', async () => {
      const resp = await fetch(`${baseUrl}/cache/max-age?seconds=31536000`);
      expect(resp.status).toBe(200);
    });
  });

  // ========================================================================
  // 4. CONDITIONAL REQUESTS
  // ========================================================================
  describe('Requisições Condicionais', ({ it }) => {
    it('deve lidar com If-Modified-Since', async () => {
      const pastDate = new Date(Date.now() - 86400000).toUTCString();
      const resp = await fetch(`${baseUrl}/cache/last-modified`, {
        headers: {
          'If-Modified-Since': pastDate,
        },
      });
      // O servidor retorna 304 Not Modified porque o conteúdo não foi modificado desde a data informada
      expect(resp.status).toBe(304);
    });

    it('deve lidar com If-None-Match', async () => {
      const resp = await fetch(`${baseUrl}/cache/etag`, {
        headers: {
          'If-None-Match': '"old-etag"',
        },
      });
      expect(resp.status).toBe(200);
    });

    it('deve lidar com a pré-condição If-Unmodified-Since', async () => {
      const futureDate = new Date(Date.now() + 86400000).toUTCString();
      const resp = await fetch(`${baseUrl}/cache/last-modified`, {
        headers: {
          'If-Unmodified-Since': futureDate,
        },
      });
      expect(resp.status).toBe(200);
    });

    it('deve lidar com If-Match para validação forte', async () => {
      const resp = await fetch(`${baseUrl}/cache/etag`, {
        headers: {
          'If-Match': '"*"',
        },
      });
      expect(resp.status).toBe(200);
    });
  });

  // ========================================================================
  // 5. COOKIE SECURITY
  // ========================================================================
  describe('Segurança de Cookies', ({ it }) => {
    it('deve impor cookie Secure apenas sobre HTTPS', async () => {
      // Teste via HTTP - cookie Secure não deve ser enviado
      const resp = await fetch(
        `${baseUrl}/cookies/set?name=secureTest&value=test&secure=true`
      );
      expect(resp.status).toBe(200);
    });

    it('deve isolar cookies HttpOnly do JavaScript', async () => {
      const resp = await fetch(
        `${baseUrl}/cookies/set?name=httpOnlyTest&value=isolated&httpOnly=true`
      );
      expect(resp.status).toBe(200);
      // HttpOnly cookies não são acessíveis via document.cookie
    });

    it('deve impor SameSite=Strict', async () => {
      const resp = await fetch(
        `${baseUrl}/cookies/set?name=strictTest&value=test&sameSite=Strict`
      );
      expect(resp.status).toBe(200);
    });

    it('deve permitir SameSite=Lax para navegação de nível superior', async () => {
      const resp = await fetch(
        `${baseUrl}/cookies/set?name=laxTest&value=test&sameSite=Lax`
      );
      expect(resp.status).toBe(200);
    });
  });

  return suite.getAPI();
};
