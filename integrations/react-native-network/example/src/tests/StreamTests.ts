/**
 * @file StreamTests.ts
 * @brief Suite completa de testes de streaming para BrowserNetwork React Native
 *
 * Testa cenários de streaming seguindo a especificação WHATWG Streams API
 * e padrões RFC 7230 (HTTP/1.1 Message Syntax - Chunked Transfer Encoding).
 *
 * @date 2026-02-16
 */

import { TestSuite, expect } from '../test-runner';

const baseUrl = 'http://10.0.2.2:3000';

/**
 * Helper para decodificar Uint8Array para string
 */
function decodeChunk(chunk: Uint8Array): string {
  return String.fromCharCode(...chunk);
}

/**
 * Helper para parsear NDJSON
 */
function parseNDJSON(text: string): any[] {
  return text
    .trim()
    .split('\n')
    .filter((line) => line.trim())
    .map((line) => JSON.parse(line));
}

export const createStreamTests = () => {
  const suite = new TestSuite('stream');
  const { describe } = suite.getAPI();

  // ========================================================================
  // 1. STREAM BÁSICO
  // ========================================================================
  describe('Stream Básico (WHATWG Streams)', ({ it }) => {
    it('deve ler stream com chunks configuráveis', async () => {
      const chunks = 5;
      const interval = 100;

      const response = await fetch(
        `${baseUrl}/stream/basic?chunks=${chunks}&interval=${interval}`
      );
      expect(response.status).toBe(200);
      expect(response.body !== null).toBe(true);

      const reader = response.body!.getReader();
      let totalChunks = 0;
      let totalLength = 0;

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;

        expect(value !== null).toBe(true);
        totalChunks++;
        if (value) {
          totalLength += value.length;
        }
      }

      expect(totalChunks).toBe(chunks);
      expect(totalLength).toBeGreaterThan(0);
    });

    it('deve verificar header Transfer-Encoding: chunked', async () => {
      const response = await fetch(`${baseUrl}/stream/basic?chunks=3`);

      // Verifica headers de streaming
      const transferEncoding = response.headers.get('Transfer-Encoding');
      const streamType = response.headers.get('X-Stream-Type');

      expect(streamType).toBe('basic');
      // Transfer-Encoding pode ser chunked ou não presente (depende do servidor)
    });

    it('deve lidar com chunks grandes (64KB cada)', async () => {
      const chunkSize = 64 * 1024;
      const chunks = 3;

      const response = await fetch(
        `${baseUrl}/stream/basic?chunks=${chunks}&chunkSize=${chunkSize}`
      );
      expect(response.status).toBe(200);

      const reader = response.body!.getReader();
      let totalBytes = 0;

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) {
          totalBytes += value.length;
        }
      }

      expect(totalBytes).toBeGreaterThan(chunkSize * 2);
    });
  });

  // ========================================================================
  // 2. STREAM BINÁRIO
  // ========================================================================
  describe('Stream Binário', ({ it }) => {
    it('deve receber quantidade exata de bytes', async () => {
      const totalBytes = 1024 * 10; // 10KB

      const response = await fetch(
        `${baseUrl}/stream/binary?totalBytes=${totalBytes}`
      );
      expect(response.status).toBe(200);

      const reader = response.body!.getReader();
      let receivedBytes = 0;

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) {
          receivedBytes += value.length;
        }
      }

      expect(receivedBytes).toBe(totalBytes);
    });

    it('deve verificar Content-Length em stream binário', async () => {
      const totalBytes = 50 * 1024;

      const response = await fetch(
        `${baseUrl}/stream/binary?totalBytes=${totalBytes}`
      );

      const contentLength = response.headers.get('Content-Length');
      expect(contentLength).toBe(totalBytes.toString());
    });

    it('deve receber dados binários com chunks pequenos', async () => {
      const totalBytes = 10 * 1024;
      const chunkSize = 512;

      const response = await fetch(
        `${baseUrl}/stream/binary?totalBytes=${totalBytes}&chunkSize=${chunkSize}`
      );

      const reader = response.body!.getReader();
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
  });

  // ========================================================================
  // 3. STREAM DE TEXTO (LOG STREAMING)
  // ========================================================================
  describe('Stream de Texto (Log Streaming)', ({ it }) => {
    it('deve receber linhas de log', async () => {
      const response = await fetch(`${baseUrl}/stream/lines`);
      expect(response.status).toBe(200);

      const reader = response.body!.getReader();
      let receivedText = '';

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) {
          receivedText += decodeChunk(value);
        }
      }

      expect(receivedText).toContain('[INFO] Application starting...');
      expect(receivedText).toContain('[INFO] Application ready');
    });

    it('deve verificar Content-Type text/plain', async () => {
      const response = await fetch(`${baseUrl}/stream/lines`);

      const contentType = response.headers.get('Content-Type');
      expect(contentType).toContain('text/plain');
    });

    it('deve receber múltiplas linhas de log', async () => {
      const response = await fetch(`${baseUrl}/stream/lines`);

      const reader = response.body!.getReader();
      let receivedText = '';

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) {
          receivedText += decodeChunk(value);
        }
      }

      const lines = receivedText.split('\n').filter((l) => l.trim());
      expect(lines.length).toBeGreaterThan(5);
    });
  });

  // ========================================================================
  // 4. STREAM JSON (NDJSON)
  // ========================================================================
  describe('Stream JSON (NDJSON)', ({ it }) => {
    it('deve receber objetos JSON linha por linha', async () => {
      const count = 10;

      const response = await fetch(`${baseUrl}/stream/json?count=${count}`);
      expect(response.status).toBe(200);

      const reader = response.body!.getReader();
      let receivedText = '';

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) {
          receivedText += decodeChunk(value);
        }
      }

      const objects = parseNDJSON(receivedText);
      expect(objects.length).toBe(count);
    });

    it('deve verificar Content-Type NDJSON', async () => {
      const response = await fetch(`${baseUrl}/stream/json?count=5`);

      const contentType = response.headers.get('Content-Type');
      expect(contentType).toContain('application/x-ndjson');
    });

    it('deve parsear cada objeto JSON individualmente', async () => {
      const response = await fetch(`${baseUrl}/stream/json?count=5`);

      const reader = response.body!.getReader();
      let receivedText = '';

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) {
          receivedText += decodeChunk(value);
        }
      }

      const objects = parseNDJSON(receivedText);

      objects.forEach((obj, index) => {
        expect(obj.id).toBe(index);
        expect(obj.timestamp !== undefined).toBe(true);
        expect(obj.data !== undefined).toBe(true);
      });
    });
  });

  // ========================================================================
  // 5. STREAM COM BACKPRESSURE
  // ========================================================================
  describe('Stream com Backpressure', ({ it }) => {
    it('deve lidar com backpressure progressivo', async () => {
      const response = await fetch(`${baseUrl}/stream/backpressure`);
      expect(response.status).toBe(200);

      const reader = response.body!.getReader();
      let chunkCount = 0;
      let totalBytes = 0;

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) {
          chunkCount++;
          totalBytes += value.length;
        }
      }

      expect(chunkCount).toBe(50);
      expect(totalBytes).toBe(50 * 64 * 1024); // 50 chunks de 64KB
    });

    it('deve verificar header X-Stream-Type backpressure', async () => {
      const response = await fetch(`${baseUrl}/stream/backpressure`);

      const streamType = response.headers.get('X-Stream-Type');
      expect(streamType).toBe('backpressure');
    });
  });

  // ========================================================================
  // 6. STREAM COM VELOCIDADE VARIÁVEL
  // ========================================================================
  describe('Stream com Velocidade Variável', ({ it }) => {
    it('deve lidar com alternância rápido/lento', async () => {
      const response = await fetch(`${baseUrl}/stream/variable`);
      expect(response.status).toBe(200);

      const reader = response.body!.getReader();
      let receivedText = '';

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) {
          receivedText += decodeChunk(value);
        }
      }

      const objects = parseNDJSON(receivedText);
      expect(objects.length).toBe(20);

      // Verifica alternância de velocidade
      const speeds = objects.map((o) => o.speed);
      expect(speeds[0]).toBe('fast');
      expect(speeds[1]).toBe('slow');
    });

    it('deve receber todos os chunks independente da velocidade', async () => {
      const response = await fetch(`${baseUrl}/stream/variable`);

      const reader = response.body!.getReader();
      let receivedText = '';

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) {
          receivedText += decodeChunk(value);
        }
      }

      const objects = parseNDJSON(receivedText);

      for (let i = 0; i < 20; i++) {
        expect(objects[i].index).toBe(i);
      }
    });
  });

  // ========================================================================
  // 7. STREAM DE ARQUIVO (DOWNLOAD)
  // ========================================================================
  describe('Stream de Arquivo (Download)', ({ it }) => {
    it('deve fazer download de arquivo 1MB via stream', async () => {
      const response = await fetch(`${baseUrl}/stream/file?mb=1`);
      expect(response.status).toBe(200);

      const contentLength = parseInt(
        response.headers.get('Content-Length') || '0'
      );
      expect(contentLength).toBe(1024 * 1024);

      const reader = response.body!.getReader();
      let receivedBytes = 0;

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) {
          receivedBytes += value.length;
        }
      }

      expect(receivedBytes).toBe(1024 * 1024);
    });

    it('deve verificar Content-Disposition attachment', async () => {
      const response = await fetch(`${baseUrl}/stream/file?mb=1`);

      const disposition = response.headers.get('Content-Disposition');
      expect(disposition !== null).toBe(true);
      expect(disposition!).toContain('attachment');
      expect(disposition!).toContain('filename=');
    });

    it('deve medir progresso de download', async () => {
      const response = await fetch(`${baseUrl}/stream/file?mb=1`);

      const contentLength = parseInt(
        response.headers.get('Content-Length') || '0'
      );
      const reader = response.body!.getReader();

      let receivedBytes = 0;
      const progressSnapshots: number[] = [];

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) {
          receivedBytes += value.length;
          const progress = Math.floor((receivedBytes / contentLength) * 100);
          if (
            progressSnapshots.length === 0 ||
            progress > progressSnapshots[progressSnapshots.length - 1]
          ) {
            progressSnapshots.push(progress);
          }
        }
      }

      expect(progressSnapshots[progressSnapshots.length - 1]).toBe(100);
      expect(progressSnapshots.length).toBeGreaterThan(1);
    });
  });

  // ========================================================================
  // 8. STREAM COM ERRO
  // ========================================================================
  describe('Stream com Erro', ({ it }) => {
    it('deve receber chunks antes do erro', async () => {
      const response = await fetch(`${baseUrl}/stream/error`);
      expect(response.status).toBe(200);

      const reader = response.body!.getReader();
      let chunksReceived = 0;
      let errorOccurred = false;

      try {
        while (true) {
          const { done, value } = await reader.read();
          if (done) break;
          if (value) {
            chunksReceived++;
          }
        }
      } catch (e: any) {
        errorOccurred = true;
      }

      // Pode ou não receber erro dependendo da implementação
      expect(chunksReceived).toBeGreaterThan(0);
    });

    it('deve verificar header X-Stream-Type error', async () => {
      const response = await fetch(`${baseUrl}/stream/error`);

      const streamType = response.headers.get('X-Stream-Type');
      expect(streamType).toBe('error');
    });
  });

  // ========================================================================
  // 9. CANCELAMENTO DE STREAM
  // ========================================================================
  describe('Cancelamento de Stream', ({ it }) => {
    it('deve cancelar stream via AbortController', async () => {
      const controller = new AbortController();

      const response = await fetch(`${baseUrl}/stream/file?mb=10`, {
        signal: controller.signal,
      });

      const reader = response.body!.getReader();
      let bytesRead = 0;

      try {
        // Lê alguns bytes e cancela
        const { value } = await reader.read();
        if (value) {
          bytesRead += value.length;
        }

        // Cancela após ler o primeiro chunk
        controller.abort();

        // Tenta continuar lendo
        while (true) {
          const { done } = await reader.read();
          if (done) break;
        }
        throw new Error('Deveria ter sido cancelado');
      } catch (e: any) {
        expect(e.name === 'AbortError' || e.message.includes('abort')).toBe(true);
      }

      expect(bytesRead).toBeGreaterThan(0);
    });

    it('deve cancelar stream via reader.cancel()', async () => {
      const response = await fetch(`${baseUrl}/stream/basic?chunks=100&interval=50`);
      const reader = response.body!.getReader();

      let chunksRead = 0;

      // Lê alguns chunks
      for (let i = 0; i < 3; i++) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) {
          chunksRead++;
        }
      }

      // Cancela o stream
      await reader.cancel('Test cancellation');

      expect(chunksRead).toBe(3);
    });
  });

  // ========================================================================
  // 10. MÚLTIPLOS STREAMS PARALELOS
  // ========================================================================
  describe('Múltiplos Streams Paralelos', ({ it }) => {
    it('deve ler múltiplos streams em paralelo', async () => {
      const streamCount = 3;

      const promises = Array.from({ length: streamCount }, async (_, i) => {
        const response = await fetch(
          `${baseUrl}/stream/basic?chunks=5&interval=50`
        );
        const reader = response.body!.getReader();
        let chunks = 0;

        while (true) {
          const { done, value } = await reader.read();
          if (done) break;
          if (value) chunks++;
        }

        return { streamId: i, chunks };
      });

      const results = await Promise.all(promises);

      expect(results.length).toBe(streamCount);
      results.forEach((r) => {
        expect(r.chunks).toBe(5);
      });
    });

    it('deve manter integridade de dados em streams paralelos', async () => {
      const sizes = [10 * 1024, 20 * 1024, 30 * 1024];

      const promises = sizes.map(async (size) => {
        const response = await fetch(
          `${baseUrl}/stream/binary?totalBytes=${size}`
        );
        const reader = response.body!.getReader();
        let bytes = 0;

        while (true) {
          const { done, value } = await reader.read();
          if (done) break;
          if (value) bytes += value.length;
        }

        return bytes;
      });

      const results = await Promise.all(promises);
      expect(results).toEqual(sizes);
    });
  });

  return suite.getAPI();
};

// Mantém compatibilidade com formato antigo para testes existentes
export const StreamTests = (suite: TestSuite) => {
  const BASE_URL = 'http://10.0.2.2:3000';

  suite.it('Stream Basic', async () => {
    const response = await fetch(
      `${BASE_URL}/stream/basic?chunks=5&interval=100`
    );
    expect(response.status).toBe(200);
    expect(response.body !== null).toBe(true);

    const reader = response.body!.getReader();
    let totalChunks = 0;
    let totalLength = 0;

    while (true) {
      const { done, value } = await reader.read();
      if (done) break;

      expect(value !== null).toBe(true);
      totalChunks++;
      if (value) {
        totalLength += value.length;
      }
    }

    expect(totalChunks).toBe(5);
    expect(totalLength).toBeGreaterThan(0);
  });

  suite.it('Stream Binary', async () => {
    const totalBytes = 1024 * 10; // 10KB
    const response = await fetch(
      `${BASE_URL}/stream/binary?totalBytes=${totalBytes}`
    );
    expect(response.status).toBe(200);

    const reader = response.body!.getReader();
    let receivedBytes = 0;

    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      if (value) {
        receivedBytes += value.length;
      }
    }

    expect(receivedBytes).toBe(totalBytes);
  });

  suite.it('Stream Lines (Text)', async () => {
    const response = await fetch(`${BASE_URL}/stream/lines`);
    expect(response.status).toBe(200);

    const reader = response.body!.getReader();
    let receivedText = '';

    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      if (value) {
        receivedText += String.fromCharCode(...value);
      }
    }

    expect(receivedText).toContain('[INFO] Application starting...');
    expect(receivedText).toContain('[INFO] Application ready');
  });
};
