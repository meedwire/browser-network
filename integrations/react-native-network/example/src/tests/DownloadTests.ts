/**
 * @file DownloadTests.ts
 * @brief Suite completa de testes de download para BrowserNetwork React Native
 *
 * Testa cenários de download seguindo padrões RFC 7231 (HTTP Semantics)
 * e RFC 7233 (Range Requests).
 *
 * @date 2026-02-16
 */

import { TestSuite, expect } from '../test-runner';

const baseUrl = 'http://10.0.2.2:3000';

export const createDownloadTests = () => {
  const suite = new TestSuite('download');
  const { describe } = suite.getAPI();

  // ========================================================================
  // 1. DOWNLOAD BINÁRIO BÁSICO
  // ========================================================================
  describe('Download Binário Básico', ({ it }) => {
    it('deve baixar conteúdo binário pequeno (1KB)', async () => {
      const resp = await fetch(`${baseUrl}/content/binary?size=1024`);
      expect(resp.status).toBe(200);
      expect(resp.headers.get('Content-Type')).toBe('application/octet-stream');

      const buffer = await resp.arrayBuffer();
      expect(buffer.byteLength).toBe(1024);
    });

    it('deve baixar conteúdo binário médio (100KB)', async () => {
      const size = 100 * 1024;
      const resp = await fetch(`${baseUrl}/content/binary?size=${size}`);
      expect(resp.status).toBe(200);

      const buffer = await resp.arrayBuffer();
      expect(buffer.byteLength).toBe(size);
    });

    it('deve baixar arquivo grande (1MB)', async () => {
      const resp = await fetch(`${baseUrl}/large/download?mb=1`);
      expect(resp.status).toBe(200);

      const buffer = await resp.arrayBuffer();
      expect(buffer.byteLength).toBe(1024 * 1024);
    });

    it('deve verificar Content-Length no header', async () => {
      const size = 50 * 1024;
      const resp = await fetch(`${baseUrl}/content/binary?size=${size}`);

      const contentLength = resp.headers.get('Content-Length');
      expect(contentLength).toBe(size.toString());
    });

    it('deve verificar Content-Disposition para attachment', async () => {
      const resp = await fetch(`${baseUrl}/large/download?mb=1`);
      const disposition = resp.headers.get('Content-Disposition');

      expect(disposition !== null).toBe(true);
      expect(disposition!).toContain('attachment');
      expect(disposition!).toContain('filename=');
    });
  });

  // ========================================================================
  // 2. DOWNLOAD COM STREAMING
  // ========================================================================
  describe('Download com Streaming', ({ it }) => {
    it('deve baixar arquivo via stream e calcular tamanho total', async () => {
      const resp = await fetch(`${baseUrl}/stream/file?mb=1`);
      expect(resp.status).toBe(200);
      expect(resp.body !== null).toBe(true);

      const reader = resp.body!.getReader();
      let totalBytes = 0;

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) {
          totalBytes += value.length;
        }
      }

      expect(totalBytes).toBe(1024 * 1024);
    });

    it('deve baixar stream binário com chunks contados', async () => {
      const totalBytes = 50 * 1024; // 50KB
      const chunkSize = 1024; // 1KB chunks
      const resp = await fetch(
        `${baseUrl}/stream/binary?totalBytes=${totalBytes}&chunkSize=${chunkSize}`
      );
      expect(resp.status).toBe(200);

      const reader = resp.body!.getReader();
      let receivedBytes = 0;
      let chunkCount = 0;

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) {
          receivedBytes += value.length;
          chunkCount++;
        }
      }

      expect(receivedBytes).toBe(totalBytes);
      expect(chunkCount).toBeGreaterThan(1);
    });

    it('deve verificar progresso de download via stream', async () => {
      const totalBytes = 100 * 1024;
      const resp = await fetch(
        `${baseUrl}/stream/binary?totalBytes=${totalBytes}&chunkSize=10240`
      );

      const contentLength = parseInt(
        resp.headers.get('Content-Length') || '0'
      );
      expect(contentLength).toBe(totalBytes);

      const reader = resp.body!.getReader();
      let receivedBytes = 0;
      const progressUpdates: number[] = [];

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) {
          receivedBytes += value.length;
          const progress = (receivedBytes / contentLength) * 100;
          progressUpdates.push(Math.floor(progress));
        }
      }

      expect(progressUpdates.length).toBeGreaterThan(0);
      expect(progressUpdates[progressUpdates.length - 1]).toBe(100);
    });
  });

  // ========================================================================
  // 3. RANGE REQUESTS (RFC 7233)
  // ========================================================================
  describe('Range Requests (RFC 7233)', ({ it }) => {
    it('deve suportar requisição com Range header', async () => {
      const resp = await fetch(`${baseUrl}/range`, {
        headers: {
          Range: 'bytes=0-99',
        },
      });

      expect(resp.status).toBe(206);
      const text = await resp.text();
      expect(text.length).toBe(100);
    });

    it('deve retornar Content-Range correto', async () => {
      const resp = await fetch(`${baseUrl}/range`, {
        headers: {
          Range: 'bytes=100-199',
        },
      });

      expect(resp.status).toBe(206);
      const contentRange = resp.headers.get('Content-Range');
      expect(contentRange !== null).toBe(true);
      expect(contentRange!).toContain('bytes 100-199');
    });

    it('deve retornar Accept-Ranges para recursos que suportam', async () => {
      const resp = await fetch(`${baseUrl}/range`);
      expect(resp.status).toBe(200);

      const acceptRanges = resp.headers.get('Accept-Ranges');
      expect(acceptRanges).toBe('bytes');
    });

    it('deve baixar parte final do arquivo', async () => {
      // Baixa os últimos 50 bytes
      const resp = await fetch(`${baseUrl}/range`, {
        headers: {
          Range: 'bytes=950-999',
        },
      });

      expect(resp.status).toBe(206);
      const text = await resp.text();
      expect(text.length).toBe(50);
    });

    it('deve retornar 200 quando Range não é especificado', async () => {
      const resp = await fetch(`${baseUrl}/range`);
      expect(resp.status).toBe(200);

      const text = await resp.text();
      expect(text.length).toBe(1000);
    });
  });

  // ========================================================================
  // 4. DOWNLOAD COM DIFERENTES CONTENT-TYPES
  // ========================================================================
  describe('Download com Diferentes Content-Types', ({ it }) => {
    it('deve baixar JSON e parsear corretamente', async () => {
      const resp = await fetch(`${baseUrl}/content/json`);
      expect(resp.status).toBe(200);
      expect(resp.headers.get('Content-Type')).toContain('application/json');

      const data = await resp.json();
      expect(data.message).toBe('JSON response');
      expect(data.data !== undefined).toBe(true);
    });

    it('deve baixar texto puro', async () => {
      const resp = await fetch(`${baseUrl}/content/text`);
      expect(resp.status).toBe(200);
      expect(resp.headers.get('Content-Type')).toContain('text/plain');

      const text = await resp.text();
      expect(text).toContain('text');
    });

    it('deve baixar HTML', async () => {
      const resp = await fetch(`${baseUrl}/content/html`);
      expect(resp.status).toBe(200);
      expect(resp.headers.get('Content-Type')).toContain('text/html');

      const html = await resp.text();
      expect(html).toContain('<html>');
      expect(html).toContain('</html>');
    });
  });

  // ========================================================================
  // 5. DOWNLOAD COM COMPRESSÃO
  // ========================================================================
  describe('Download com Compressão', ({ it }) => {
    it('deve aceitar conteúdo comprimido com Accept-Encoding', async () => {
      const resp = await fetch(`${baseUrl}/compression/auto`, {
        headers: {
          'Accept-Encoding': 'gzip, deflate, br',
        },
      });
      expect(resp.status).toBe(200);

      // O body deve ser descomprimido automaticamente
      const data = await resp.json();
      expect(data.message !== undefined).toBe(true);
      expect(data.data.length).toBeGreaterThan(0);
    });

    it('deve lidar com payload grande comprimido', async () => {
      const resp = await fetch(`${baseUrl}/compression/auto`);
      expect(resp.status).toBe(200);

      const data = await resp.json();
      expect(data.data.length).toBe(1000);
    });
  });

  // ========================================================================
  // 6. CENÁRIOS DE ERRO EM DOWNLOAD
  // ========================================================================
  describe('Cenários de Erro em Download', ({ it }) => {
    it('deve lidar com 404 em download', async () => {
      const resp = await fetch(`${baseUrl}/nonexistent/file.bin`);
      expect(resp.status).toBe(404);
      expect(resp.ok).toBeFalsy();
    });

    it('deve lidar com timeout em download lento', async () => {
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), 2000);

      try {
        await fetch(`${baseUrl}/delay?ms=5000`, {
          signal: controller.signal,
        });
        throw new Error('Deveria ter abortado');
      } catch (e: any) {
        expect(e.name === 'AbortError' || e.message.includes('abort')).toBe(true);
      } finally {
        clearTimeout(timeoutId);
      }
    });

    it('deve lidar com cancelamento de download via AbortController', async () => {
      const controller = new AbortController();

      const promise = fetch(`${baseUrl}/stream/file?mb=10`, {
        signal: controller.signal,
      });

      // Cancela após iniciar
      setTimeout(() => controller.abort(), 100);

      try {
        const resp = await promise;
        const reader = resp.body!.getReader();

        // Tenta ler - deve falhar
        while (true) {
          const { done } = await reader.read();
          if (done) break;
        }
        throw new Error('Deveria ter sido cancelado');
      } catch (e: any) {
        expect(e.name === 'AbortError' || e.message.includes('abort')).toBe(true);
      }
    });
  });

  // ========================================================================
  // 7. DOWNLOAD SEQUENCIAL E PARALELO
  // ========================================================================
  describe('Download Sequencial e Paralelo', ({ it }) => {
    it('deve baixar múltiplos arquivos em sequência', async () => {
      const sizes = [1024, 2048, 4096];
      const results: number[] = [];

      for (const size of sizes) {
        const resp = await fetch(`${baseUrl}/content/binary?size=${size}`);
        const buffer = await resp.arrayBuffer();
        results.push(buffer.byteLength);
      }

      expect(results).toEqual(sizes);
    });

    it('deve baixar múltiplos arquivos em paralelo', async () => {
      const sizes = [1024, 2048, 4096, 8192];

      const promises = sizes.map(async (size) => {
        const resp = await fetch(`${baseUrl}/content/binary?size=${size}`);
        const buffer = await resp.arrayBuffer();
        return buffer.byteLength;
      });

      const results = await Promise.all(promises);

      expect(results.length).toBe(4);
      expect(results).toEqual(sizes);
    });

    it('deve manter integridade de dados em downloads paralelos', async () => {
      // Faz 5 downloads paralelos e verifica que cada um tem o tamanho correto
      const count = 5;
      const size = 10 * 1024;

      const promises = Array.from({ length: count }, () =>
        fetch(`${baseUrl}/content/binary?size=${size}`).then((r) =>
          r.arrayBuffer()
        )
      );

      const results = await Promise.all(promises);

      results.forEach((buffer) => {
        expect(buffer.byteLength).toBe(size);
      });
    });
  });

  return suite.getAPI();
};
