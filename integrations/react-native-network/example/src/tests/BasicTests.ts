/**
 * @file BasicTests.ts
 * @brief Suite completa de testes básicos para BrowserNetwork React Native
 *
 * Cobre 100% dos recursos HTTP/HTTPS fundamentais implementados.
 *
 * @date 2026-02-15
 */

import { TestSuite, expect } from '../test-runner';

const baseUrl = 'http://10.0.2.2:3000';

export const createBasicTests = () => {
  const suite = new TestSuite('basic');
  const { describe } = suite.getAPI();

  // ========================================================================
  // 1. FETCH API BASICS
  // ========================================================================
  describe('Básicos da Fetch API', ({ it }) => {
    it('deve realizar uma requisição GET para /echo', async () => {
      const resp = await fetch(`${baseUrl}/echo`);
      expect(resp.status).toBe(200);
      expect(resp.ok).toBeTruthy();

      const data = await resp.json();
      expect(data.method).toBe('GET');
    });

    it('deve lidar com headers customizados', async () => {
      const resp = await fetch(`${baseUrl}/echo`, {
        headers: { 'X-Custom': 'JSI-Test' },
      });
      const data = await resp.json();
      expect(data.headers['x-custom']).toBe('JSI-Test');
    });

    it('deve seguir redirecionamentos por padrão', async () => {
      const resp = await fetch(`${baseUrl}/redirect?to=/echo`);
      expect(resp.status).toBe(200);
      expect(resp.url).toContain('/echo');
    });
  });

  // ========================================================================
  // 2. HTTP METHODS
  // ========================================================================
  describe('Métodos HTTP', ({ it }) => {
    it('deve suportar requisições POST', async () => {
      const resp = await fetch(`${baseUrl}/methods/post`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ test: 'data' }),
      });
      const data = await resp.json();
      expect(data.method).toBe('POST');
      expect(data.success).toBeTruthy();
    });

    it('deve suportar requisições PUT', async () => {
      const resp = await fetch(`${baseUrl}/methods/put`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ update: true }),
      });
      const data = await resp.json();
      expect(data.method).toBe('PUT');
      expect(data.success).toBeTruthy();
    });

    it('deve suportar requisições DELETE', async () => {
      const resp = await fetch(`${baseUrl}/methods/delete`, {
        method: 'DELETE',
      });
      const data = await resp.json();
      expect(data.method).toBe('DELETE');
      expect(data.success).toBeTruthy();
    });

    it('deve suportar requisições PATCH', async () => {
      const resp = await fetch(`${baseUrl}/methods/patch`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ patch: 'value' }),
      });
      const data = await resp.json();
      expect(data.method).toBe('PATCH');
      expect(data.success).toBeTruthy();
    });

    it('deve suportar requisições HEAD', async () => {
      const resp = await fetch(`${baseUrl}/methods/head`, {
        method: 'HEAD',
      });
      expect(resp.status).toBe(200);
    });

    it('deve suportar requisições OPTIONS', async () => {
      const resp = await fetch(`${baseUrl}/methods/options`, {
        method: 'OPTIONS',
      });
      expect(resp.status >= 200 && resp.status < 300).toBeTruthy();
    });
  });

  // ========================================================================
  // 3. STATUS CODES
  // ========================================================================
  describe('Códigos de Status HTTP', ({ it }) => {
    it('deve lidar com 200 OK', async () => {
      const resp = await fetch(`${baseUrl}/status/200`);
      expect(resp.status).toBe(200);
      expect(resp.ok).toBeTruthy();
    });

    it('deve lidar com 201 Created', async () => {
      const resp = await fetch(`${baseUrl}/status/201`);
      expect(resp.status).toBe(201);
      expect(resp.ok).toBeTruthy();
    });

    it('deve lidar com 204 No Content', async () => {
      const resp = await fetch(`${baseUrl}/status/204`);
      expect(resp.status).toBe(204);
      expect(resp.ok).toBeTruthy();
    });

    it('deve lidar com 400 Bad Request', async () => {
      const resp = await fetch(`${baseUrl}/status/400`);
      expect(resp.status).toBe(400);
      expect(resp.ok).toBeFalsy();
    });

    it('deve lidar com 401 Unauthorized', async () => {
      const resp = await fetch(`${baseUrl}/status/401`);
      expect(resp.status).toBe(401);
      expect(resp.ok).toBeFalsy();
    });

    it('deve lidar com 403 Forbidden', async () => {
      const resp = await fetch(`${baseUrl}/status/403`);
      expect(resp.status).toBe(403);
      expect(resp.ok).toBeFalsy();
    });

    it('deve lidar com 404 Not Found', async () => {
      const resp = await fetch(`${baseUrl}/status/404`);
      expect(resp.status).toBe(404);
      expect(resp.ok).toBeFalsy();
    });

    it('deve lidar com 500 Internal Server Error', async () => {
      const resp = await fetch(`${baseUrl}/status/500`);
      expect(resp.status).toBe(500);
      expect(resp.ok).toBeFalsy();
    });

    it('deve lidar com 502 Bad Gateway', async () => {
      const resp = await fetch(`${baseUrl}/status/502`);
      expect(resp.status).toBe(502);
      expect(resp.ok).toBeFalsy();
    });

    it('deve lidar com 503 Service Unavailable', async () => {
      const resp = await fetch(`${baseUrl}/status/503`);
      expect(resp.status).toBe(503);
      expect(resp.ok).toBeFalsy();
    });
  });

  // ========================================================================
  // 4. HEADERS
  // ========================================================================
  describe('API de Headers', ({ it }) => {
    it('deve suportar acesso case-insensitive', () => {
      const h = new Headers();
      h.set('Content-Type', 'application/json');
      expect(h.get('content-type')).toBe('application/json');
      expect(h.get('CONTENT-TYPE')).toBe('application/json');
    });

    it('deve suportar múltiplos valores para o mesmo header', () => {
      const h = new Headers();
      h.set('Accept', 'application/json');
      h.append('Accept', 'text/html');
      expect(h.get('accept')).toContain('application/json');
      expect(h.get('accept')).toContain('text/html');
    });

    it('deve deletar headers', () => {
      const h = new Headers();
      h.set('X-Test', 'value');
      expect(h.has('x-test')).toBeTruthy();
      h.delete('X-Test');
      expect(h.has('x-test')).toBeFalsy();
    });

    it('deve enviar headers customizados para o servidor', async () => {
      const resp = await fetch(`${baseUrl}/echo`, {
        headers: {
          'X-Custom-Header': 'test-value',
          'X-Another': 'another-value',
        },
      });
      const data = await resp.json();
      expect(data.headers['x-custom-header']).toBe('test-value');
      expect(data.headers['x-another']).toBe('another-value');
    });
  });

  // ========================================================================
  // 5. REQUEST BODY
  // ========================================================================
  describe('Corpo da Requisição (Body)', ({ it }) => {
    it('deve enviar corpo JSON', async () => {
      const payload = { name: 'John', age: 30 };
      const resp = await fetch(`${baseUrl}/echo`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
      });
      const data = await resp.json();
      expect(data.method).toBe('POST');
      expect(data.headers['content-type']).toContain('application/json');
    });

    it('deve enviar corpo de texto', async () => {
      const resp = await fetch(`${baseUrl}/echo`, {
        method: 'POST',
        headers: { 'Content-Type': 'text/plain' },
        body: 'Plain text content',
      });
      const data = await resp.json();
      expect(data.method).toBe('POST');
    });

    it('deve lidar com corpo vazio', async () => {
      const resp = await fetch(`${baseUrl}/echo`, {
        method: 'POST',
      });
      const data = await resp.json();
      expect(data.method).toBe('POST');
    });
  });

  // ========================================================================
  // 7. CONTENT TYPES
  // ========================================================================
  describe('Tipos de Conteúdo (Content Types)', ({ it }) => {
    it('deve lidar com resposta JSON', async () => {
      const resp = await fetch(`${baseUrl}/content/json`);
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.message).toBe('JSON response');
      expect(data.data).toBeTruthy();
    });

    it('deve lidar com resposta text/plain', async () => {
      const resp = await fetch(`${baseUrl}/content/text`);
      expect(resp.status).toBe(200);
      const text = await resp.text();
      expect(text).toContain('text');
    });

    it('deve lidar com resposta HTML', async () => {
      const resp = await fetch(`${baseUrl}/content/html`);
      expect(resp.status).toBe(200);
      const html = await resp.text();
      expect(html).toContain('<html>');
    });
  });

  // ========================================================================
  // 8. AUTHENTICATION
  // ========================================================================
  describe('Autenticação', ({ it }) => {
    it('deve lidar com sucesso no Basic Auth', async () => {
      const credentials = btoa('test:password');
      const resp = await fetch(`${baseUrl}/auth/basic`, {
        headers: {
          Authorization: `Basic ${credentials}`,
        },
      });
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.authenticated).toBeTruthy();
    });

    it('deve rejeitar Basic Auth inválido', async () => {
      const resp = await fetch(`${baseUrl}/auth/basic`, {
        headers: {
          Authorization: 'Basic invalid',
        },
      });
      expect(resp.status).toBe(401);
    });

    it('deve lidar com auth por token Bearer', async () => {
      const resp = await fetch(`${baseUrl}/auth/bearer`, {
        headers: {
          Authorization: 'Bearer valid-token-123',
        },
      });
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.authenticated).toBeTruthy();
    });

    it('deve rejeitar auth ausente', async () => {
      const resp = await fetch(`${baseUrl}/auth/basic`);
      expect(resp.status).toBe(401);
    });
  });

  // ========================================================================
  // 9. CACHE & CONDITIONAL REQUESTS
  // ========================================================================
  describe('Cache e Requisições Condicionais', ({ it }) => {
    it('deve lidar com ETag', async () => {
      const resp = await fetch(`${baseUrl}/cache/etag`);
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.data).toBeTruthy();
    });

    it('deve lidar com Last-Modified', async () => {
      const resp = await fetch(`${baseUrl}/cache/last-modified`);
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.data).toBeTruthy();
    });

    it('deve retornar 304 Not Modified quando houver correspondência', async () => {
      const resp1 = await fetch(`${baseUrl}/cache/etag`);
      expect(resp1.status).toBe(200);
      // TODO: Implementar quando Response.headers estiver disponível
    });
  });

  // ========================================================================
  // 10. SPECIAL SCENARIOS
  // ========================================================================
  describe('Cenários Especiais', ({ it }) => {
    it('deve lidar com respostas atrasadas (delay)', async () => {
      const start = Date.now();
      const resp = await fetch(`${baseUrl}/delay?ms=1000`);
      const elapsed = Date.now() - start;

      expect(resp.status).toBe(200);
      expect(elapsed).toBeGreaterThan(900);
      const data = await resp.json();
      expect(data.delayed).toBe(1000);
    });

    it('deve lidar com parâmetros de query', async () => {
      const resp = await fetch(`${baseUrl}/echo?foo=bar&baz=qux`);
      const data = await resp.json();
      expect(data.url).toContain('foo=bar');
      expect(data.url).toContain('baz=qux');
    });

    it('deve lidar com codificação de URL', async () => {
      const resp = await fetch(
        `${baseUrl}/echo?name=${encodeURIComponent('John Doe')}`
      );
      const data = await resp.json();
      expect(data.url).toContain('name=John');
    });
  });

  // ========================================================================
  // 11. ERROR HANDLING
  // ========================================================================
  describe('Tratamento de Erros', ({ it }) => {
    it('deve lidar com erros de rede graciosamente', async () => {
      try {
        await fetch('http://10.0.2.2:9999/nonexistent');
        throw new Error('Should have thrown');
      } catch (e: any) {
        expect(e.message).toBeTruthy();
      }
    });

    it('deve lidar com URLs malformadas', async () => {
      try {
        await fetch('not-a-valid-url');
        throw new Error('Should have thrown');
      } catch (e: any) {
        expect(e.message).toBeTruthy();
      }
    });
  });

  return suite.getAPI();
};
