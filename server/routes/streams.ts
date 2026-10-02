/**
 * @file streams.ts
 * @description Rotas WebStreams (WHATWG Streams API) para testes
 *
 * Implementa testes para ReadableStream conforme especificação WHATWG:
 * - Streaming de dados em chunks
 * - Backpressure
 * - Cancelamento de streams
 * - Streams com diferentes tamanhos de chunk
 * - Streams com delay entre chunks
 * - Streams de texto e binário
 */

import { getCorsHeaders } from "../utils/cors";
import { delay, generateBinaryData } from "../utils/helpers";

/**
 * Stream básico - envia chunks de dados
 * Query params: ?chunks=10&chunkSize=1024&interval=100
 */
export async function handleStreamBasic(req: Request): Promise<Response> {
  const url = new URL(req.url);
  const chunks = parseInt(url.searchParams.get("chunks") || "10");
  const chunkSize = parseInt(url.searchParams.get("chunkSize") || "1024");
  const interval = parseInt(url.searchParams.get("interval") || "100");

  const stream = new ReadableStream({
    async start(controller) {
      for (let i = 0; i < chunks; i++) {
        await delay(interval);

        const chunk = JSON.stringify({
          index: i,
          total: chunks,
          timestamp: Date.now(),
          data: "x".repeat(chunkSize),
        }) + "\n";

        controller.enqueue(new TextEncoder().encode(chunk));
      }

      controller.close();
    },
    cancel(reason) {
      console.log("Stream cancelled:", reason);
    },
  });

  return new Response(stream, {
    headers: {
      ...getCorsHeaders(req),
      "Content-Type": "application/octet-stream",
      "Transfer-Encoding": "chunked",
      "X-Stream-Type": "basic",
    },
  });
}

/**
 * Stream binário - envia dados binários em chunks
 * Query params: ?totalBytes=1048576&chunkSize=8192
 */
export async function handleStreamBinary(req: Request): Promise<Response> {
  const url = new URL(req.url);
  const totalBytes = parseInt(url.searchParams.get("totalBytes") || "1048576"); // 1MB default
  const chunkSize = parseInt(url.searchParams.get("chunkSize") || "8192"); // 8KB chunks

  const stream = new ReadableStream({
    async start(controller) {
      let bytesSent = 0;

      while (bytesSent < totalBytes) {
        const remainingBytes = totalBytes - bytesSent;
        const currentChunkSize = Math.min(chunkSize, remainingBytes);

        const chunk = generateBinaryData(currentChunkSize);
        controller.enqueue(chunk);

        bytesSent += currentChunkSize;

        // Pequeno delay para simular rede
        await delay(10);
      }

      controller.close();
    },
  });

  return new Response(stream, {
    headers: {
      ...getCorsHeaders(req),
      "Content-Type": "application/octet-stream",
      "Content-Length": totalBytes.toString(),
      "X-Stream-Type": "binary",
    },
  });
}

/**
 * Stream com backpressure
 * Simula backpressure controlando a velocidade de envio
 */
export async function handleStreamBackpressure(req: Request): Promise<Response> {
  const stream = new ReadableStream({
    async start(controller) {
      const chunks = 50;
      const chunkSize = 1024 * 64; // 64KB chunks

      for (let i = 0; i < chunks; i++) {
        // Simula backpressure esperando mais tempo a cada chunk
        const backpressureDelay = Math.min(i * 10, 500);
        await delay(backpressureDelay);

        const chunk = new Uint8Array(chunkSize);
        chunk.fill(i % 256); // Preenche com padrão

        controller.enqueue(chunk);
      }

      controller.close();
    },
  });

  return new Response(stream, {
    headers: {
      ...getCorsHeaders(req),
      "Content-Type": "application/octet-stream",
      "X-Stream-Type": "backpressure",
    },
  });
}

/**
 * Stream JSON - envia objetos JSON linha por linha (NDJSON)
 * Query params: ?count=100
 */
export async function handleStreamJSON(req: Request): Promise<Response> {
  const url = new URL(req.url);
  const count = parseInt(url.searchParams.get("count") || "100");

  const stream = new ReadableStream({
    async start(controller) {
      for (let i = 0; i < count; i++) {
        await delay(50);

        const obj = {
          id: i,
          timestamp: Date.now(),
          data: {
            value: Math.random(),
            description: `Item number ${i}`,
          },
        };

        const line = JSON.stringify(obj) + "\n";
        controller.enqueue(new TextEncoder().encode(line));
      }

      controller.close();
    },
  });

  return new Response(stream, {
    headers: {
      ...getCorsHeaders(req),
      "Content-Type": "application/x-ndjson",
      "X-Stream-Type": "json",
    },
  });
}

/**
 * Stream infinito - continua enviando dados até ser cancelado
 * Útil para testar cancelamento de streams
 */
export async function handleStreamInfinite(req: Request): Promise<Response> {
  const stream = new ReadableStream({
    async start(controller) {
      let counter = 0;

      const sendChunk = async () => {
        try {
          const chunk = JSON.stringify({
            counter: counter++,
            timestamp: Date.now(),
          }) + "\n";

          controller.enqueue(new TextEncoder().encode(chunk));

          // Agenda próximo chunk
          await delay(1000);
          await sendChunk();
        } catch (error) {
          controller.close();
        }
      };

      await sendChunk();
    },
    cancel(reason) {
      console.log("Infinite stream cancelled:", reason);
    },
  });

  return new Response(stream, {
    headers: {
      ...getCorsHeaders(req),
      "Content-Type": "application/octet-stream",
      "X-Stream-Type": "infinite",
    },
  });
}

/**
 * Stream com erro
 * Envia alguns chunks e então falha
 */
export async function handleStreamError(req: Request): Promise<Response> {
  const stream = new ReadableStream({
    async start(controller) {
      // Envia alguns chunks com sucesso
      for (let i = 0; i < 5; i++) {
        await delay(200);
        controller.enqueue(
          new TextEncoder().encode(
            JSON.stringify({ index: i, status: "ok" }) + "\n",
          ),
        );
      }

      // Simula erro
      controller.error(new Error("Stream failed unexpectedly"));
    },
  });

  return new Response(stream, {
    headers: {
      ...getCorsHeaders(req),
      "Content-Type": "application/octet-stream",
      "X-Stream-Type": "error",
    },
  });
}

/**
 * Stream com variação de velocidade
 * Alterna entre envio rápido e lento
 */
export async function handleStreamVariable(req: Request): Promise<Response> {
  const stream = new ReadableStream({
    async start(controller) {
      const chunks = 20;

      for (let i = 0; i < chunks; i++) {
        // Alterna entre rápido (50ms) e lento (500ms)
        const delayTime = i % 2 === 0 ? 50 : 500;
        await delay(delayTime);

        const chunk = JSON.stringify({
          index: i,
          speed: i % 2 === 0 ? "fast" : "slow",
          delay: delayTime,
        }) + "\n";

        controller.enqueue(new TextEncoder().encode(chunk));
      }

      controller.close();
    },
  });

  return new Response(stream, {
    headers: {
      ...getCorsHeaders(req),
      "Content-Type": "application/octet-stream",
      "X-Stream-Type": "variable",
    },
  });
}

/**
 * Stream de arquivo simulado
 * Simula download de arquivo grande em chunks
 */
export async function handleStreamFile(req: Request): Promise<Response> {
  const url = new URL(req.url);
  const sizeMB = parseInt(url.searchParams.get("mb") || "10");
  const totalBytes = sizeMB * 1024 * 1024;
  const chunkSize = 64 * 1024; // 64KB chunks

  const stream = new ReadableStream({
    async start(controller) {
      let bytesSent = 0;

      while (bytesSent < totalBytes) {
        const remainingBytes = totalBytes - bytesSent;
        const currentChunkSize = Math.min(chunkSize, remainingBytes);

        const chunk = generateBinaryData(currentChunkSize);
        controller.enqueue(chunk);

        bytesSent += currentChunkSize;

        // Simula velocidade de rede realista (aproximadamente 10MB/s)
        await delay(6);
      }

      controller.close();
    },
  });

  return new Response(stream, {
    headers: {
      ...getCorsHeaders(req),
      "Content-Type": "application/octet-stream",
      "Content-Length": totalBytes.toString(),
      "Content-Disposition": `attachment; filename="test-${sizeMB}mb.bin"`,
      "X-Stream-Type": "file",
    },
  });
}

/**
 * Stream de texto linha por linha
 * Simula log streaming
 */
export async function handleStreamLines(req: Request): Promise<Response> {
  const stream = new ReadableStream({
    async start(controller) {
      const lines = [
        "[INFO] Application starting...",
        "[INFO] Loading configuration",
        "[INFO] Connecting to database",
        "[WARN] Retrying connection (attempt 1)",
        "[INFO] Database connected",
        "[INFO] Starting web server",
        "[INFO] Server listening on port 3000",
        "[INFO] Application ready",
      ];

      for (const line of lines) {
        await delay(300);
        controller.enqueue(
          new TextEncoder().encode(`${new Date().toISOString()} ${line}\n`),
        );
      }

      controller.close();
    },
  });

  return new Response(stream, {
    headers: {
      ...getCorsHeaders(req),
      "Content-Type": "text/plain",
      "X-Stream-Type": "lines",
    },
  });
}

/**
 * Rotas WebStreams exportadas
 */
export const streamRoutes = {
  "/stream/basic": handleStreamBasic,
  "/stream/binary": handleStreamBinary,
  "/stream/backpressure": handleStreamBackpressure,
  "/stream/json": handleStreamJSON,
  "/stream/infinite": handleStreamInfinite,
  "/stream/error": handleStreamError,
  "/stream/variable": handleStreamVariable,
  "/stream/file": handleStreamFile,
  "/stream/lines": handleStreamLines,
};
