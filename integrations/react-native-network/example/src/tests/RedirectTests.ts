import { TestSuite, expect } from '../test-runner';

const baseUrl = 'http://10.0.2.2:3000';
const createCookieName = (prefix: string) =>
  `${prefix}_${Date.now()}_${Math.random().toString(16).slice(2)}`;

export const RedirectTests = (suite: TestSuite) => {
  const { describe } = suite.getAPI();

  describe('Redirecionamentos (Status Codes)', ({ it }) => {
    it('deve seguir 301 Moved Permanently', async () => {
      const resp = await fetch(`${baseUrl}/redirect/301`);
      expect(resp.status).toBe(200);
      expect(resp.redirected).toBe(true);
      expect(resp.url).toContain('/echo');
    });

    it('deve seguir 302 Found', async () => {
      const resp = await fetch(`${baseUrl}/redirect/302`);
      expect(resp.status).toBe(200);
      expect(resp.redirected).toBe(true);
    });

    it('deve seguir 303 See Other', async () => {
      const resp = await fetch(`${baseUrl}/redirect/303`);
      expect(resp.status).toBe(200);
      expect(resp.redirected).toBe(true);
    });

    it('deve seguir 307 Temporary Redirect', async () => {
      const resp = await fetch(`${baseUrl}/redirect/307`);
      expect(resp.status).toBe(200);
      expect(resp.redirected).toBe(true);
    });

    it('deve seguir 308 Permanent Redirect', async () => {
      const resp = await fetch(`${baseUrl}/redirect/308`);
      expect(resp.status).toBe(200);
      expect(resp.redirected).toBe(true);
    });
  });

  describe('Redirecionamentos (Métodos)', ({ it }) => {
    it('301: deve mudar POST para GET', async () => {
      const resp = await fetch(`${baseUrl}/redirect/301`, {
        method: 'POST',
        body: JSON.stringify({ data: 'test' }),
      });
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.method).toBe('GET');
      // Body é descartado em 301 POST->GET
    });

    it('302: deve mudar POST para GET', async () => {
      const resp = await fetch(`${baseUrl}/redirect/302`, {
        method: 'POST',
        body: JSON.stringify({ data: 'test' }),
      });
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.method).toBe('GET');
    });

    it('303: deve mudar POST para GET', async () => {
      const resp = await fetch(`${baseUrl}/redirect/303`, {
        method: 'POST',
        body: JSON.stringify({ data: 'test' }),
      });
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.method).toBe('GET');
    });

    it('307: deve preservar POST e Body', async () => {
      const payload = { data: 'preserved' };
      const resp = await fetch(`${baseUrl}/redirect/307`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
      });
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.method).toBe('POST');
      expect(data.body).toEqual(payload); // Assumindo que /echo retorna o body parseado
    });

    it('308: deve preservar POST e Body', async () => {
      const payload = { data: 'preserved' };
      const resp = await fetch(`${baseUrl}/redirect/308`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
      });
      expect(resp.status).toBe(200);
      const data = await resp.json();
      expect(data.method).toBe('POST');
      expect(data.body).toEqual(payload);
    });
  });

  describe('Redirecionamentos (Modos & Cadeias)', ({ it }) => {
    it('mode: manual (deve retornar o opaqueredirect/3xx)', async () => {
      const resp = await fetch(`${baseUrl}/redirect/302`, {
        redirect: 'manual',
      });
      expect(resp.type).toBe('opaqueredirect');
      // Conforme Fetch Spec, opaqueredirect tem status 0
      expect(resp.status).toBe(0);
    });

    it('mode: error (deve falhar na rede)', async () => {
      try {
        await fetch(`${baseUrl}/redirect/302`, { redirect: 'error' });
        throw new Error('Should have thrown network error');
      } catch (e: any) {
        expect(e.message).toContain('Network request failed');
      }
    });

    it('deve seguir cadeia de redirecionamentos (A->B->C)', async () => {
      const resp = await fetch(`${baseUrl}/redirect/chain?count=3`);
      expect(resp.status).toBe(200);
      // O servidor Bun finaliza a cadeia em /redirect/chain?count=1
      expect(resp.url).toContain('/redirect/chain?count=1');
      expect(resp.redirected).toBe(true);
    });

    it('deve aplicar Set-Cookie durante redirect automático', async () => {
      const cookieName = createCookieName('redirectCookie');
      const cookieValue = 'redirected';
      const resp = await fetch(
        `${baseUrl}/redirect/cookies/set-and-forward?name=${cookieName}&value=${cookieValue}&to=${encodeURIComponent(
          '/cookies/get'
        )}&code=302`
      );

      expect(resp.status).toBe(200);
      expect(resp.redirected).toBe(true);

      const data = await resp.json();
      expect(data.cookies[cookieName]).toBe(cookieValue);

      const persistedResp = await fetch(`${baseUrl}/cookies/get`);
      expect(persistedResp.status).toBe(200);
      const persistedData = await persistedResp.json();
      expect(persistedData.cookies[cookieName]).toBe(cookieValue);
    });
  });
};
