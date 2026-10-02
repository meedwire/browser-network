import { TestSuite } from '../test-runner/TestSuite';
import { WebSocket } from '../../../src';

// Server base URL
// Android Emulator requires 10.0.2.2 to access host localhost
const BASE_URL = 'http://10.0.2.2:3000';
const WS_BASE_URL = BASE_URL.replace('http', 'ws');
const createCookieName = (prefix: string) =>
  `${prefix}_${Date.now()}_${Math.random().toString(16).slice(2)}`;

export const WebSocketTests = (suite: TestSuite) => {
  const openAndWaitForConnected = (url: string, context: string) =>
    new Promise<any>((resolve, reject) => {
      const ws = new WebSocket(url);
      let settled = false;

      const finish = (payload?: any, error?: Error) => {
        if (settled) return;
        settled = true;
        clearTimeout(timeout);
        ws.close();

        if (error) {
          reject(error);
          return;
        }

        resolve(payload);
      };

      const timeout = setTimeout(() => {
        finish(undefined, new Error(`Timeout waiting for ${context}`));
      }, 5000);

      ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          if (data.type !== 'connected') return;
          finish(data);
        } catch (error: any) {
          finish(
            undefined,
            error instanceof Error
              ? error
              : new Error(String(error ?? 'Unknown error'))
          );
        }
      };

      ws.onerror = (e) => {
        finish(
          undefined,
          new Error(`WebSocket error during ${context}: ${JSON.stringify(e)}`)
        );
      };
    });

  suite.it('WebSocket Connect & Close', async () => {
    return new Promise<void>((resolve, reject) => {
      const ws = new WebSocket(`${WS_BASE_URL}/ws/echo`);

      let opened = false;
      const timeout = setTimeout(() => {
        reject(new Error('Connection timeout'));
        ws.close();
      }, 5000);

      ws.onopen = () => {
        opened = true;

        // Verifica estado connected
        if (ws.readyState !== WebSocket.OPEN) {
          reject(
            new Error(
              `Expected readyState to be OPEN (1), got ${ws.readyState}`
            )
          );
          return;
        }

        // Fecha conexão
        ws.close(1000, 'Test complete');
      };

      ws.onclose = (event) => {
        clearTimeout(timeout);
        if (opened) {
          // Verifica código e razão se suportado (mock server pode não enviar)
          if (ws.readyState === WebSocket.CLOSED) {
            resolve();
          } else {
            reject(
              new Error(
                `Expected readyState to be CLOSED (3), got ${ws.readyState}`
              )
            );
          }
        } else {
          reject(new Error('Closed before opening'));
        }
      };

      ws.onerror = (e) => {
        clearTimeout(timeout);
        reject(new Error(`WebSocket error: ${JSON.stringify(e)}`));
      };
    });
  });

  suite.it('WebSocket Echo Text', async () => {
    return new Promise<void>((resolve, reject) => {
      const ws = new WebSocket(`${WS_BASE_URL}/ws/echo`);
      const messageText = 'Hello WebSocket';
      let messageReceived = false;

      const timeout = setTimeout(() => {
        ws.close();
        reject(new Error('Timeout waiting for echo'));
      }, 5000);

      ws.onopen = () => {
        ws.send(messageText);
      };

      ws.onmessage = (event) => {
        const data = JSON.parse(event.data);

        // O servidor de echo manda uma mensagem de boas vindas primeiro
        if (data.type === 'connected') return;

        if (data.type === 'echo' && data.message === messageText) {
          messageReceived = true;
          ws.close();
          clearTimeout(timeout);
          resolve();
        }
      };

      ws.onerror = (e) => {
        clearTimeout(timeout);
        reject(new Error(`WebSocket error: ${JSON.stringify(e)}`));
      };
    });
  });

  /*
   * Nota: O polyfill atual converte binário para string na camada JS antes de enviar.
   * Este teste verifica se o servidor recebe e ecoa, mesmo que como string/base64 por enquanto.
   */
  suite.it('WebSocket Echo Binary (as string)', async () => {
    return new Promise<void>((resolve, reject) => {
      const ws = new WebSocket(`${WS_BASE_URL}/ws/echo`);
      const binaryData = 'BinaryDataSimulated'; // Simulando dados binários como string por enquanto

      const timeout = setTimeout(() => {
        ws.close();
        reject(new Error('Timeout waiting for binary echo'));
      }, 5000);

      ws.onopen = () => {
        ws.send(binaryData);
      };

      ws.onmessage = (event) => {
        const data = JSON.parse(event.data);
        if (data.type === 'connected') return;

        if (data.type === 'echo' && data.message === binaryData) {
          ws.close();
          clearTimeout(timeout);
          resolve();
        }
      };

      ws.onerror = (e) =>
        reject(new Error(`WebSocket error: ${JSON.stringify(e)}`));
    });
  });

  suite.it('WebSocket Protocols', async () => {
    return new Promise<void>((resolve, reject) => {
      const protocols = ['chat', 'super-chat'];
      const ws = new WebSocket(`${WS_BASE_URL}/ws/echo`, protocols);

      const timeout = setTimeout(() => {
        ws.close();
        reject(new Error('Timeout waiting for protocol connection'));
      }, 5000);

      ws.onopen = () => {
        // O servidor de echo do Bun pode não negociar protocolos explicitamente neste setup simples,
        // mas a conexão deve ocorrer sem erro.
        // Se o servidor suportasse, ws.protocol deveria ser 'chat' ou 'super-chat'.
        // Por hora validamos apenas que conecta com a lista.
        ws.close();
        clearTimeout(timeout);
        resolve();
      };

      ws.onerror = (e) =>
        reject(new Error(`WebSocket error: ${JSON.stringify(e)}`));
    });
  });

  suite.it('WebSocket Ping/Pong (Heartbeat)', async () => {
    return new Promise<void>((resolve, reject) => {
      const ws = new WebSocket(`${WS_BASE_URL}/ws/ping`);

      const timeout = setTimeout(() => {
        ws.close();
        reject(new Error('Timeout waiting for ping'));
      }, 10000); // 10s pois o ping é a cada 5s

      ws.onmessage = (event) => {
        const data = JSON.parse(event.data);
        if (data.type === 'ping') {
          // Recebeu ping, envia pong (opcional, mas bom pra testar send)
          ws.send(JSON.stringify({ type: 'pong', timestamp: data.timestamp }));
          ws.close();
          clearTimeout(timeout);
          resolve();
        }
      };

      ws.onerror = (e) =>
        reject(new Error(`WebSocket error: ${JSON.stringify(e)}`));
    });
  });

  suite.it('WebSocket Handshake Cookies', async () => {
    const cookieName = createCookieName('wsCookie');
    const cookieValue = 'handshake';
    const setResp = await fetch(
      `${BASE_URL}/cookies/set?name=${cookieName}&value=${cookieValue}`
    );

    if (setResp.status !== 200) {
      throw new Error(`Expected 200 when setting cookie, got ${setResp.status}`);
    }

    return new Promise<void>((resolve, reject) => {
      const ws = new WebSocket(`${WS_BASE_URL}/ws/echo`);
      let settled = false;

      const finish = (error?: Error) => {
        if (settled) return;
        settled = true;
        clearTimeout(timeout);
        ws.close();

        if (error) {
          reject(error);
          return;
        }

        resolve();
      };

      const timeout = setTimeout(() => {
        finish(new Error('Timeout waiting for handshake cookies'));
      }, 5000);

      ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          if (data.type !== 'connected') return;

          if (!data.cookies || data.cookies[cookieName] !== cookieValue) {
            finish(
              new Error(
                `Expected handshake cookie ${cookieName}=${cookieValue}, got ${JSON.stringify(
                  data.cookies
                )}`
              )
            );
            return;
          }

          finish();
        } catch (error: any) {
          finish(
            error instanceof Error ? error : new Error(String(error ?? 'Unknown error'))
          );
        }
      };

      ws.onerror = (e) => {
        finish(new Error(`WebSocket error: ${JSON.stringify(e)}`));
      };
    });
  });

  suite.it('WebSocket Handshake Set-Cookie 101 -> stored -> fetch reuse', async () => {
    const cookieName = createCookieName('wsSetCookie');
    const cookieValue = 'from101';

    await openAndWaitForConnected(
      `${WS_BASE_URL}/ws/echo?setCookieName=${encodeURIComponent(
        cookieName
      )}&setCookieValue=${encodeURIComponent(cookieValue)}`,
      '101 Set-Cookie handshake'
    );

    const snapshotResp = await fetch(`${BASE_URL}/cookies/get`);
    if (snapshotResp.status !== 200) {
      throw new Error(
        `Expected 200 when reading stored cookies, got ${snapshotResp.status}`
      );
    }

    const snapshot = await snapshotResp.json();
    if (!snapshot.cookies || snapshot.cookies[cookieName] !== cookieValue) {
      throw new Error(
        `Expected fetch to reuse stored cookie ${cookieName}=${cookieValue}, got ${JSON.stringify(
          snapshot.cookies
        )}`
      );
    }
  });

  suite.it('WebSocket Handshake Set-Cookie 101 -> stored -> websocket reuse', async () => {
    const cookieName = createCookieName('wsReuseCookie');
    const cookieValue = 'from101-reuse';

    await openAndWaitForConnected(
      `${WS_BASE_URL}/ws/echo?setCookieName=${encodeURIComponent(
        cookieName
      )}&setCookieValue=${encodeURIComponent(cookieValue)}`,
      '101 Set-Cookie handshake for websocket reuse'
    );

    const reusedHandshake = await openAndWaitForConnected(
      `${WS_BASE_URL}/ws/echo`,
      'websocket handshake cookie reuse'
    );

    if (
      !reusedHandshake.cookies ||
      reusedHandshake.cookies[cookieName] !== cookieValue
    ) {
      throw new Error(
        `Expected websocket handshake to reuse stored cookie ${cookieName}=${cookieValue}, got ${JSON.stringify(
          reusedHandshake.cookies
        )}`
      );
    }
  });

  suite.it('WebSocket Handshake Multi-Set-Cookie 101 -> websocket reuse', async () => {
    const cookieName1 = createCookieName('wsMultiCookieA');
    const cookieValue1 = 'first';
    const cookieName2 = createCookieName('wsMultiCookieB');
    const cookieValue2 = 'second';

    await openAndWaitForConnected(
      `${WS_BASE_URL}/ws/echo?setCookieName=${encodeURIComponent(
        cookieName1
      )}&setCookieValue=${encodeURIComponent(
        cookieValue1
      )}&setCookieName2=${encodeURIComponent(
        cookieName2
      )}&setCookieValue2=${encodeURIComponent(cookieValue2)}`,
      '101 multi Set-Cookie handshake'
    );

    const reusedHandshake = await openAndWaitForConnected(
      `${WS_BASE_URL}/ws/echo`,
      'websocket handshake multi-cookie reuse'
    );

    if (
      !reusedHandshake.cookies ||
      reusedHandshake.cookies[cookieName1] !== cookieValue1 ||
      reusedHandshake.cookies[cookieName2] !== cookieValue2
    ) {
      throw new Error(
        `Expected websocket handshake to reuse cookies ${cookieName1}=${cookieValue1} and ${cookieName2}=${cookieValue2}, got ${JSON.stringify(
          reusedHandshake.cookies
        )}`
      );
    }
  });

  suite.it('WebSocket Handshake HttpOnly Set-Cookie 101 -> websocket reuse', async () => {
    const cookieName = createCookieName('wsHttpOnlyCookie');
    const cookieValue = 'http-only-101';

    await openAndWaitForConnected(
      `${WS_BASE_URL}/ws/echo?setCookieName=${encodeURIComponent(
        cookieName
      )}&setCookieValue=${encodeURIComponent(
        cookieValue
      )}&setCookieHttpOnly=true`,
      '101 HttpOnly Set-Cookie handshake'
    );

    const reusedHandshake = await openAndWaitForConnected(
      `${WS_BASE_URL}/ws/echo`,
      'websocket handshake HttpOnly cookie reuse'
    );

    if (
      !reusedHandshake.cookies ||
      reusedHandshake.cookies[cookieName] !== cookieValue
    ) {
      throw new Error(
        `Expected websocket handshake to reuse HttpOnly cookie ${cookieName}=${cookieValue}, got ${JSON.stringify(
          reusedHandshake.cookies
        )}`
      );
    }
  });

  suite.it('WebSocket Connection Error', async () => {
    return new Promise<void>((resolve, reject) => {
      // Porta inválida para forçar erro
      const ws = new WebSocket('ws://localhost:9999/invalid');

      const timeout = setTimeout(() => {
        ws.close(); // Cleanup
        reject(new Error('Timeout waiting for error'));
      }, 5000);

      ws.onopen = () => {
        clearTimeout(timeout);
        ws.close();
        reject(new Error('Should not connect to invalid port'));
      };

      ws.onerror = (e) => {
        clearTimeout(timeout);
        // Sucesso se der erro
        resolve();
      };
    });
  });
};
