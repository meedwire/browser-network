/**
 * @file websocket.ts
 * @description Handlers WebSocket para testes completos
 *
 * Implementa testes para WebSocket conforme especificação W3C:
 * - Echo (texto e binário)
 * - Broadcast / Pub-Sub
 * - Room-based messaging
 * - Ping/Pong (heartbeat)
 * - Binary messages
 * - Compression
 * - Backpressure
 * - Erro e reconexão
 */

import type { ServerWebSocket } from "bun";
import { parseCookies } from "../utils/helpers";

/**
 * Tipos de dados do WebSocket
 */
export interface WebSocketData {
  createdAt: number;
  cookies: Record<string, string>;
  cookieHeader?: string | null;
  userId?: string;
  room?: string;
  messageCount: number;
  path?: string;
}

/**
 * WebSocket Echo - simplesmente ecoa de volta as mensagens
 * Rota: /ws/echo
 */
export const echoWebSocket = {
  open(ws: ServerWebSocket<WebSocketData>) {
    console.log(`[Echo] Client connected: ${ws.data.userId}`);
    ws.send(
      JSON.stringify({
        type: "connected",
        message: "Welcome to echo server",
        cookies: ws.data.cookies,
        timestamp: Date.now(),
      }),
    );
  },

  message(ws: ServerWebSocket<WebSocketData>, message: string | Buffer) {
    ws.data.messageCount++;

    // Echo back - suporta texto e binário
    if (typeof message === "string") {
      ws.send(
        JSON.stringify({
          type: "echo",
          message,
          count: ws.data.messageCount,
          timestamp: Date.now(),
        }),
      );
    } else {
      // Binary echo
      ws.send(message);
    }
  },

  close(ws: ServerWebSocket<WebSocketData>, code: number, reason: string) {
    console.log(`[Echo] Client disconnected: code=${code}, reason=${reason}`);
  },

  drain(ws: ServerWebSocket<WebSocketData>) {
    console.log("[Echo] Backpressure relieved");
  },
};

/**
 * WebSocket Broadcast - broadcast para todos os clientes
 * Usa Pub/Sub nativo do Bun
 * Rota: /ws/broadcast
 */
export const broadcastWebSocket = {
  open(ws: ServerWebSocket<WebSocketData>) {
    ws.subscribe("broadcast");
    console.log("[Broadcast] Client connected and subscribed");

    ws.send(
      JSON.stringify({
        type: "connected",
        message: "Connected to broadcast server",
      }),
    );
  },

  message(ws: ServerWebSocket<WebSocketData>, message: string | Buffer) {
    if (typeof message === "string") {
      try {
        const data = JSON.parse(message);

        // Broadcast para todos os subscribers (exceto o sender)
        ws.publish(
          "broadcast",
          JSON.stringify({
            type: "broadcast",
            from: ws.data.userId || "anonymous",
            message: data.message,
            timestamp: Date.now(),
          }),
        );
      } catch (error) {
        ws.send(
          JSON.stringify({
            type: "error",
            message: "Invalid JSON",
          }),
        );
      }
    }
  },

  close(ws: ServerWebSocket<WebSocketData>) {
    ws.unsubscribe("broadcast");
    console.log("[Broadcast] Client disconnected");
  },
};

/**
 * WebSocket Room - múltiplas rooms com pub/sub
 * Clientes podem entrar/sair de rooms
 * Rota: /ws/room
 */
export const roomWebSocket = {
  open(ws: ServerWebSocket<WebSocketData>) {
    console.log("[Room] Client connected");
    ws.send(
      JSON.stringify({
        type: "connected",
        message: "Connected to room server. Use 'join' to enter a room.",
      }),
    );
  },

  message(ws: ServerWebSocket<WebSocketData>, message: string | Buffer) {
    if (typeof message !== "string") return;

    try {
      const data = JSON.parse(message);

      switch (data.action) {
        case "join":
          // Sai da room anterior se estiver em uma
          if (ws.data.room) {
            ws.unsubscribe(ws.data.room);
          }

          // Entra na nova room
          ws.data.room = data.room;
          ws.subscribe(data.room);

          ws.send(
            JSON.stringify({
              type: "joined",
              room: data.room,
              timestamp: Date.now(),
            }),
          );

          // Notifica outros na room
          ws.publish(
            data.room,
            JSON.stringify({
              type: "user-joined",
              userId: ws.data.userId || "anonymous",
              timestamp: Date.now(),
            }),
          );
          break;

        case "leave":
          if (ws.data.room) {
            // Notifica outros na room
            ws.publish(
              ws.data.room,
              JSON.stringify({
                type: "user-left",
                userId: ws.data.userId || "anonymous",
                timestamp: Date.now(),
              }),
            );

            ws.unsubscribe(ws.data.room);
            ws.data.room = undefined;

            ws.send(
              JSON.stringify({
                type: "left",
                timestamp: Date.now(),
              }),
            );
          }
          break;

        case "message":
          if (ws.data.room) {
            ws.publish(
              ws.data.room,
              JSON.stringify({
                type: "message",
                from: ws.data.userId || "anonymous",
                message: data.message,
                timestamp: Date.now(),
              }),
            );
          } else {
            ws.send(
              JSON.stringify({
                type: "error",
                message: "You must join a room first",
              }),
            );
          }
          break;

        default:
          ws.send(
            JSON.stringify({
              type: "error",
              message: `Unknown action: ${data.action}`,
            }),
          );
      }
    } catch (error) {
      ws.send(
        JSON.stringify({
          type: "error",
          message: "Invalid JSON",
        }),
      );
    }
  },

  close(ws: ServerWebSocket<WebSocketData>) {
    if (ws.data.room) {
      ws.publish(
        ws.data.room,
        JSON.stringify({
          type: "user-left",
          userId: ws.data.userId || "anonymous",
          timestamp: Date.now(),
        }),
      );
      ws.unsubscribe(ws.data.room);
    }
    console.log("[Room] Client disconnected");
  },
};

/**
 * WebSocket Binary - testa mensagens binárias
 * Rota: /ws/binary
 */
export const binaryWebSocket = {
  open(ws: ServerWebSocket<WebSocketData>) {
    console.log("[Binary] Client connected");

    // Envia mensagem de boas-vindas em texto
    ws.send(
      JSON.stringify({
        type: "connected",
        message: "Send binary data to receive statistics",
      }),
    );
  },

  message(ws: ServerWebSocket<WebSocketData>, message: string | Buffer) {
    if (Buffer.isBuffer(message)) {
      // Processa dados binários
      const size = message.length;
      const sum = message.reduce((acc, byte) => acc + byte, 0);
      const avg = sum / size;

      // Responde com estatísticas em JSON
      ws.send(
        JSON.stringify({
          type: "binary-stats",
          size,
          sum,
          average: avg.toFixed(2),
          timestamp: Date.now(),
        }),
      );

      // Também ecoa os dados binários de volta
      ws.send(message);
    } else {
      ws.send(
        JSON.stringify({
          type: "info",
          message: "Send binary data to see statistics",
        }),
      );
    }
  },

  close(ws: ServerWebSocket<WebSocketData>) {
    console.log("[Binary] Client disconnected");
  },
};

/**
 * WebSocket Ping/Pong - testa heartbeat
 * Envia pings periódicos
 * Rota: /ws/ping
 */
export const pingWebSocket = {
  open(ws: ServerWebSocket<WebSocketData>) {
    console.log("[Ping] Client connected");

    ws.send(
      JSON.stringify({
        type: "connected",
        message: "Ping/pong test server",
      }),
    );

    // Envia ping a cada 5 segundos
    const intervalId = setInterval(() => {
      try {
        ws.send(
          JSON.stringify({
            type: "ping",
            timestamp: Date.now(),
          }),
        );
      } catch (error) {
        clearInterval(intervalId);
      }
    }, 5000);

    // Guarda o intervalId para cleanup (em implementação real, usar WeakMap)
    (ws as any).pingInterval = intervalId;
  },

  message(ws: ServerWebSocket<WebSocketData>, message: string | Buffer) {
    if (typeof message === "string") {
      try {
        const data = JSON.parse(message);

        if (data.type === "pong") {
          ws.send(
            JSON.stringify({
              type: "pong-received",
              latency: Date.now() - data.timestamp,
              timestamp: Date.now(),
            }),
          );
        }
      } catch (error) {
        // Ignora mensagens inválidas
      }
    }
  },

  close(ws: ServerWebSocket<WebSocketData>) {
    // Cleanup do interval
    if ((ws as any).pingInterval) {
      clearInterval((ws as any).pingInterval);
    }
    console.log("[Ping] Client disconnected");
  },
};

/**
 * WebSocket Stress - testa alta frequência de mensagens
 * Rota: /ws/stress
 */
export const stressWebSocket = {
  open(ws: ServerWebSocket<WebSocketData>) {
    console.log("[Stress] Client connected");
    ws.send(
      JSON.stringify({
        type: "connected",
        message: 'Send {"action": "start"} to begin stress test',
      }),
    );
  },

  message(ws: ServerWebSocket<WebSocketData>, message: string | Buffer) {
    if (typeof message !== "string") return;

    try {
      const data = JSON.parse(message);

      if (data.action === "start") {
        const count = data.count || 1000;
        const intervalMs = data.interval || 0;

        // Envia muitas mensagens rapidamente
        let sent = 0;
        const sendBatch = () => {
          const batchSize = Math.min(100, count - sent);

          for (let i = 0; i < batchSize; i++) {
            const result = ws.send(
              JSON.stringify({
                type: "data",
                index: sent + i,
                timestamp: Date.now(),
              }),
            );

            // Verifica backpressure
            if (result === -1) {
              console.log("[Stress] Backpressure detected");
            }
          }

          sent += batchSize;

          if (sent < count) {
            if (intervalMs > 0) {
              setTimeout(sendBatch, intervalMs);
            } else {
              setImmediate(sendBatch);
            }
          } else {
            ws.send(
              JSON.stringify({
                type: "complete",
                sent,
                timestamp: Date.now(),
              }),
            );
          }
        };

        sendBatch();
      }
    } catch (error) {
      ws.send(
        JSON.stringify({
          type: "error",
          message: "Invalid JSON",
        }),
      );
    }
  },

  close(ws: ServerWebSocket<WebSocketData>) {
    console.log("[Stress] Client disconnected");
  },

  drain(ws: ServerWebSocket<WebSocketData>) {
    console.log("[Stress] Backpressure relieved, can send more data");
  },
};

/**
 * Mapa de rotas WebSocket
 */
export const webSocketHandlers: Record<string, any> = {
  "/ws/echo": echoWebSocket,
  "/ws/broadcast": broadcastWebSocket,
  "/ws/room": roomWebSocket,
  "/ws/binary": binaryWebSocket,
  "/ws/ping": pingWebSocket,
  "/ws/stress": stressWebSocket,
};

/**
 * Função helper para criar dados iniciais do WebSocket
 */
export function createWebSocketData(req: Request): WebSocketData {
  const url = new URL(req.url);
  const userId = url.searchParams.get("userId") || crypto.randomUUID();
  const cookieHeader = req.headers.get("Cookie");

  return {
    createdAt: Date.now(),
    cookies: parseCookies(cookieHeader),
    cookieHeader,
    userId,
    messageCount: 0,
  };
}
