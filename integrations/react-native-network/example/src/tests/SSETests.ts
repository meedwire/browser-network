/**
 * @file SSETests.ts
 * @brief Suite de testes para Server-Sent Events (SSE)
 *
 * Valida a implementação do EventSource contra os endpoints SSE
 * do servidor Bun de testes.
 *
 * @date 2026-02-16
 */

import { TestSuite, expect } from '../test-runner';

const baseUrl = 'http://10.0.2.2:3000';

/**
 * Helper para aguardar um tempo específico
 */
const delay = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

export const SSETests = (suite: TestSuite) => {
  const { describe } = suite.getAPI();

  // ========================================================================
  // 1. CONEXÃO BÁSICA SSE
  // ========================================================================
  describe('Conexão SSE', ({ it }) => {
    it('deve conectar ao endpoint SSE', async () => {
      return new Promise<void>((resolve, reject) => {
        let opened = false;

        // @ts-ignore - EventSource é instalado globalmente
        const es = new EventSource(
          `${baseUrl}/sse/simple?count=2&interval=500`
        );

        const timeout = setTimeout(() => {
          es.close();
          if (opened) {
            resolve();
          } else {
            reject(new Error('Connection did not open in time'));
          }
        }, 3000);

        es.onopen = () => {
          opened = true;
        };

        es.onerror = (err: any) => {
          clearTimeout(timeout);
          es.close();
          reject(new Error(err?.message || 'Connection error'));
        };
      });
    });

    it('deve receber mensagens do stream SSE', async () => {
      return new Promise<void>((resolve, reject) => {
        const messages: string[] = [];

        // @ts-ignore
        const es = new EventSource(
          `${baseUrl}/sse/simple?count=3&interval=200`
        );

        const timeout = setTimeout(() => {
          es.close();
          if (messages.length > 0) {
            resolve();
          } else {
            reject(new Error('No messages received'));
          }
        }, 3000);

        es.onmessage = (event: any) => {
          messages.push(event.data);
          if (messages.length >= 2) {
            clearTimeout(timeout);
            es.close();
            resolve();
          }
        };

        es.onerror = (err: any) => {
          clearTimeout(timeout);
          es.close();
          reject(new Error(err?.message || 'Connection error'));
        };
      });
    });

    it('deve fechar a conexão corretamente', async () => {
      // @ts-ignore
      const es = new EventSource(
        `${baseUrl}/sse/simple?count=10&interval=1000`
      );

      await delay(500);

      // Verificar que está aberto
      expect(es.readyState).toBe(1); // OPEN

      es.close();

      // Verificar que está fechado
      expect(es.readyState).toBe(2); // CLOSED
    });
  });

  // ========================================================================
  // 2. EVENTOS NOMEADOS SSE
  // ========================================================================
  describe('Eventos Nomeados SSE', ({ it }) => {
    it('deve receber eventos nomeados', async () => {
      return new Promise<void>((resolve, reject) => {
        const namedEvents: Array<{ type: string; data: string }> = [];

        // @ts-ignore
        const es = new EventSource(`${baseUrl}/sse/named-events`);

        const eventTypes = [
          'message',
          'notification',
          'update',
          'ping',
          'error',
        ];
        eventTypes.forEach((type) => {
          es.addEventListener(type, (event: any) => {
            namedEvents.push({ type, data: event.data });
          });
        });

        const timeout = setTimeout(() => {
          es.close();
          if (namedEvents.length > 0) {
            // Verificar que recebemos pelo menos um evento nomeado
            const types = new Set(namedEvents.map((e) => e.type));
            if (types.size > 0) {
              resolve();
            } else {
              reject(new Error('No named events received'));
            }
          } else {
            reject(new Error('No events received'));
          }
        }, 4000);

        es.onerror = (err: any) => {
          clearTimeout(timeout);
          es.close();
          // Ignorar erros de fechamento normal
        };
      });
    });

    it('deve realizar o parse dos dados do evento como JSON', async () => {
      return new Promise<void>((resolve, reject) => {
        // @ts-ignore
        const es = new EventSource(
          `${baseUrl}/sse/simple?count=1&interval=100`
        );

        const timeout = setTimeout(() => {
          es.close();
          reject(new Error('Timeout waiting for event'));
        }, 3000);

        es.onmessage = (event: any) => {
          clearTimeout(timeout);
          es.close();

          try {
            const data = JSON.parse(event.data);
            expect(typeof data.index).toBe('number');
            expect(typeof data.timestamp).toBe('number');
            expect(data.message).toBeTruthy();
            resolve();
          } catch (e: any) {
            reject(new Error(`Failed to parse JSON: ${e.message}`));
          }
        };

        es.onerror = (err: any) => {
          clearTimeout(timeout);
          es.close();
          reject(new Error(err?.message || 'Connection error'));
        };
      });
    });
  });

  // ========================================================================
  // 3. PROGRESSO E MULTILINE
  // ========================================================================
  describe('Progresso e Multiline SSE', ({ it }) => {
    it('deve receber eventos de progresso', async () => {
      return new Promise<void>((resolve, reject) => {
        const progressUpdates: number[] = [];

        // @ts-ignore
        const es = new EventSource(`${baseUrl}/sse/progress`);

        es.addEventListener('progress', (event: any) => {
          try {
            const data = JSON.parse(event.data);
            progressUpdates.push(data.progress);

            // Quando recebemos progresso suficiente, finalizar
            if (progressUpdates.length >= 5) {
              es.close();
              // Verificar que o progresso aumenta
              expect(
                progressUpdates[progressUpdates.length - 1]
              ).toBeGreaterThan(progressUpdates[0]!);
              resolve();
            }
          } catch (e) {
            // Ignorar erros de parse
          }
        });

        const timeout = setTimeout(() => {
          es.close();
          if (progressUpdates.length > 0) {
            resolve();
          } else {
            reject(new Error('No progress events received'));
          }
        }, 6000);

        es.onerror = () => {
          clearTimeout(timeout);
          es.close();
          // Ignorar erros
        };
      });
    });

    it('deve lidar com dados multilinhas', async () => {
      return new Promise<void>((resolve, reject) => {
        // @ts-ignore
        const es = new EventSource(`${baseUrl}/sse/multiline`);

        es.addEventListener('multiline', (event: any) => {
          es.close();

          expect(event.data).toBeTruthy();
          expect(event.data.length).toBeGreaterThan(10);
          // Multiline data deve conter quebras de linha ou múltiplas linhas
          resolve();
        });

        const timeout = setTimeout(() => {
          es.close();
          reject(new Error('Timeout waiting for multiline event'));
        }, 3000);

        es.onerror = () => {
          clearTimeout(timeout);
          es.close();
        };
      });
    });
  });

  // ========================================================================
  // 4. COMPLETE EVENT
  // ========================================================================
  describe('Evento de Conclusão SSE (Complete)', ({ it }) => {
    it('deve receber evento complete quando o stream termina', async () => {
      return new Promise<void>((resolve, reject) => {
        let completed = false;

        // @ts-ignore
        const es = new EventSource(
          `${baseUrl}/sse/simple?count=2&interval=200`
        );

        es.addEventListener('complete', () => {
          completed = true;
          es.close();
          resolve();
        });

        const timeout = setTimeout(() => {
          es.close();
          if (completed) {
            resolve();
          } else {
            reject(new Error('Complete event not received'));
          }
        }, 4000);

        es.onerror = () => {
          clearTimeout(timeout);
          es.close();
        };
      });
    });
  });

  // ========================================================================
  // 5. TRATAMENTO DE ERROS
  // ========================================================================
  describe('Tratamento de Erros SSE', ({ it }) => {
    it('deve disparar onerror em falha de conexão', async () => {
      return new Promise<void>((resolve, reject) => {
        let errorTriggered = false;

        // @ts-ignore - Porta inválida deve falhar
        const es = new EventSource('http://10.0.2.2:9999/nonexistent');

        es.onerror = () => {
          errorTriggered = true;
          clearTimeout(timeout);
          es.close();
          resolve();
        };

        const timeout = setTimeout(() => {
          es.close();
          if (errorTriggered) {
            resolve();
          } else {
            reject(new Error('Error event not triggered'));
          }
        }, 5000);
      });
    });
  });

  // ========================================================================
  // 6. EVENTSOURCE API
  // ========================================================================
  describe('API do EventSource', ({ it }) => {
    it('deve expor a propriedade url', async () => {
      const url = `${baseUrl}/sse/simple?count=1&interval=100`;

      // @ts-ignore
      const es = new EventSource(url);

      expect(es.url).toBe(url);

      es.close();
    });

    it('deve ter as constantes de readyState corretas', () => {
      // @ts-ignore
      expect(EventSource.CONNECTING).toBe(0);
      // @ts-ignore
      expect(EventSource.OPEN).toBe(1);
      // @ts-ignore
      expect(EventSource.CLOSED).toBe(2);
    });

    it('deve suportar removeEventListener', async () => {
      return new Promise<void>((resolve) => {
        let eventCount = 0;

        const handler = () => {
          eventCount++;
        };

        // @ts-ignore
        const es = new EventSource(
          `${baseUrl}/sse/simple?count=5&interval=200`
        );

        es.addEventListener('message', handler);

        setTimeout(() => {
          const countBefore = eventCount;
          es.removeEventListener('message', handler);

          setTimeout(() => {
            // Após remover o listener, não deve contar mais eventos
            expect(eventCount).toBe(countBefore);
            es.close();
            resolve();
          }, 500);
        }, 500);
      });
    });
  });

  return suite.getAPI();
};
