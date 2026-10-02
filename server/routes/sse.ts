/**
 * @file sse.ts
 * @description Rotas Server-Sent Events (SSE) para testes
 *
 * Implementa rotas SSE conforme especificação W3C EventSource:
 * - Eventos simples
 * - Eventos nomeados (event: type)
 * - IDs de eventos (id: unique-id)
 * - Reconexão automática (retry: milliseconds)
 * - Múltiplos eventos em stream
 * - Eventos com dados multiline
 */

import { getCorsHeaders } from "../utils/cors";
import { delay } from "../utils/helpers";

/**
 * Helper para criar mensagens SSE formatadas
 */
function createSSEMessage(data: string, event?: string, id?: string): string {
  let message = "";

  if (event) {
    message += `event: ${event}\n`;
  }

  if (id) {
    message += `id: ${id}\n`;
  }

  // Suporta dados multiline
  const lines = data.split("\n");
  for (const line of lines) {
    message += `data: ${line}\n`;
  }

  message += "\n";
  return message;
}

/**
 * SSE simples - envia mensagens a cada intervalo
 * Query params: ?count=10&interval=1000
 */
export async function handleSSESimple(req: Request): Promise<Response> {
  const url = new URL(req.url);
  const count = parseInt(url.searchParams.get("count") || "10");
  const interval = parseInt(url.searchParams.get("interval") || "1000");

  const stream = new ReadableStream({
    async start(controller) {
      // Envia header de retry (reconexão automática)
      controller.enqueue(`retry: ${interval}\n\n`);

      for (let i = 0; i < count; i++) {
        await delay(interval);

        const message = createSSEMessage(
          JSON.stringify({
            index: i,
            timestamp: Date.now(),
            message: `Message ${i + 1} of ${count}`,
          }),
          undefined,
          `msg-${i}`,
        );

        controller.enqueue(message);
      }

      // Envia evento de conclusão
      controller.enqueue(createSSEMessage("Stream completed", "complete"));
      controller.close();
    },
  });

  return new Response(stream, {
    headers: {
      ...getCorsHeaders(req),
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache, no-transform",
      Connection: "keep-alive",
      "X-Accel-Buffering": "no", // Nginx support
    },
  });
}

/**
 * SSE com eventos nomeados
 * Testa diferentes tipos de eventos (message, notification, update, error)
 */
export async function handleSSENamedEvents(req: Request): Promise<Response> {
  const stream = new ReadableStream({
    async start(controller) {
      const events = [
        { type: "message", data: { text: "Hello from SSE" } },
        { type: "notification", data: { title: "New notification", priority: "high" } },
        { type: "update", data: { version: "1.0.1", changes: ["fix", "feature"] } },
        { type: "ping", data: { timestamp: Date.now() } },
        { type: "error", data: { code: 500, message: "Test error event" } },
      ];

      for (let i = 0; i < events.length; i++) {
        await delay(500);

        const event = events[i];
        const message = createSSEMessage(
          JSON.stringify(event.data),
          event.type,
          `event-${i}`,
        );

        controller.enqueue(message);
      }

      controller.close();
    },
  });

  return new Response(stream, {
    headers: {
      ...getCorsHeaders(req),
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      Connection: "keep-alive",
    },
  });
}

/**
 * SSE infinito - mantém conexão aberta enviando heartbeat
 * Útil para testar reconexão e keep-alive
 */
export async function handleSSEInfinite(req: Request): Promise<Response> {
  const stream = new ReadableStream({
    async start(controller) {
      let counter = 0;

      // Envia heartbeat a cada 5 segundos
      const intervalId = setInterval(() => {
        try {
          const message = createSSEMessage(
            JSON.stringify({
              type: "heartbeat",
              counter: counter++,
              timestamp: Date.now(),
            }),
            "heartbeat",
            `hb-${counter}`,
          );

          controller.enqueue(message);
        } catch (error) {
          clearInterval(intervalId);
          controller.close();
        }
      }, 5000);

      // Cleanup quando a conexão é fechada
      req.signal?.addEventListener("abort", () => {
        clearInterval(intervalId);
        controller.close();
      });
    },
  });

  return new Response(stream, {
    headers: {
      ...getCorsHeaders(req),
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      Connection: "keep-alive",
    },
  });
}

/**
 * SSE com dados multiline
 * Testa capacidade de processar eventos com múltiplas linhas de dados
 */
export async function handleSSEMultiline(req: Request): Promise<Response> {
  const stream = new ReadableStream({
    async start(controller) {
      await delay(500);

      const multilineData = `Line 1: First line of data
Line 2: Second line of data
Line 3: Third line of data
{
  "json": "can also be multiline",
  "with": "proper formatting"
}`;

      const message = createSSEMessage(multilineData, "multiline", "ml-1");
      controller.enqueue(message);

      controller.close();
    },
  });

  return new Response(stream, {
    headers: {
      ...getCorsHeaders(req),
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      Connection: "keep-alive",
    },
  });
}

/**
 * SSE com simulação de erro e reconexão
 * Fecha a conexão abruptamente após alguns eventos
 */
export async function handleSSEError(req: Request): Promise<Response> {
  const stream = new ReadableStream({
    async start(controller) {
      // Envia alguns eventos normais
      for (let i = 0; i < 3; i++) {
        await delay(500);
        controller.enqueue(
          createSSEMessage(JSON.stringify({ index: i }), "message", `msg-${i}`),
        );
      }

      // Simula erro fechando a stream abruptamente
      controller.error(new Error("Connection lost"));
    },
  });

  return new Response(stream, {
    headers: {
      ...getCorsHeaders(req),
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      Connection: "keep-alive",
      // Retry após 2 segundos
      "retry": "2000",
    },
  });
}

/**
 * SSE com progresso de tarefa
 * Simula uma tarefa com progresso de 0 a 100%
 */
export async function handleSSEProgress(req: Request): Promise<Response> {
  const stream = new ReadableStream({
    async start(controller) {
      const steps = 20;

      for (let i = 0; i <= steps; i++) {
        await delay(200);

        const progress = Math.round((i / steps) * 100);
        const message = createSSEMessage(
          JSON.stringify({
            progress,
            step: i,
            total: steps,
            status: progress === 100 ? "completed" : "processing",
          }),
          "progress",
          `progress-${i}`,
        );

        controller.enqueue(message);
      }

      // Evento final
      controller.enqueue(createSSEMessage("Task completed successfully", "complete"));
      controller.close();
    },
  });

  return new Response(stream, {
    headers: {
      ...getCorsHeaders(req),
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      Connection: "keep-alive",
    },
  });
}

/**
 * Rotas SSE exportadas
 */
export const sseRoutes = {
  "/sse/simple": handleSSESimple,
  "/sse/named-events": handleSSENamedEvents,
  "/sse/infinite": handleSSEInfinite,
  "/sse/multiline": handleSSEMultiline,
  "/sse/error": handleSSEError,
  "/sse/progress": handleSSEProgress,
};
