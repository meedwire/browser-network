# Servidor de Testes - Browser Network

Servidor Bun completo para testar 100% das funcionalidades da biblioteca de rede React Native.

## 📋 Visão Geral

Este servidor implementa todas as funcionalidades necessárias para testar completamente a biblioteca C++ de rede:

- **48 Rotas HTTP/HTTPS** - Todos os métodos, headers, cookies, cache, redirects, etc
- **6 Rotas SSE** - Server-Sent Events com eventos nomeados, multiline, progress
- **9 Rotas WebStreams** - Streaming com backpressure, binary, JSON, cancelamento
- **6 Rotas WebSocket** - Echo, broadcast, rooms, binary, ping/pong, stress tests

**Total: 69 endpoints** implementados seguindo as especificações W3C e WHATWG.

## 🚀 Início Rápido

### Instalação

```bash
cd server
bun install
```

### Executar

```bash
# Modo normal
bun start

# Modo desenvolvimento (com hot reload)
bun dev
```

O servidor estará disponível em `http://localhost:3000`

## 📚 Documentação das Rotas

### HTTP/HTTPS Routes

#### Básicas

```bash
# Health check
curl http://localhost:3000/health

# Echo - retorna informações da requisição
curl http://localhost:3000/echo
curl -X POST http://localhost:3000/echo -H "Content-Type: application/json" -d '{"test": "data"}'
```

#### Métodos HTTP

```bash
# GET
curl http://localhost:3000/methods/get

# POST
curl -X POST http://localhost:3000/methods/post -d '{"data": "test"}' -H "Content-Type: application/json"

# PUT
curl -X PUT http://localhost:3000/methods/put -d '{"data": "test"}' -H "Content-Type: application/json"

# DELETE
curl -X DELETE http://localhost:3000/methods/delete

# PATCH
curl -X PATCH http://localhost:3000/methods/patch -d '{"data": "test"}' -H "Content-Type: application/json"

# HEAD
curl -I http://localhost:3000/methods/head

# OPTIONS
curl -X OPTIONS http://localhost:3000/methods/options
```

#### Delay e Timeout

```bash
# Delay de 2 segundos
curl "http://localhost:3000/delay?ms=2000"

# Timeout (nunca responde - para testes de timeout)
curl --max-time 5 http://localhost:3000/timeout/never
```

#### Status Codes

```bash
# Status codes específicos
curl http://localhost:3000/status/200
curl http://localhost:3000/status/404
curl http://localhost:3000/status/500
```

#### Redirects

```bash
# Redirect 302 (seguindo automaticamente)
curl -L http://localhost:3000/redirect/302

# Redirect customizável
curl -L "http://localhost:3000/redirect?to=/echo&code=307"

# Cadeia de redirects
curl -L "http://localhost:3000/redirect/chain?count=5"

# Redirect que define cookie antes de seguir
curl -L -c cookies.txt "http://localhost:3000/redirect/cookies/set-and-forward?name=redirect_cookie&value=ok&to=/cookies/get"
```

#### Cookies

```bash
# Definir cookie
curl -c cookies.txt http://localhost:3000/cookies/set

# Ler cookies
curl -b cookies.txt http://localhost:3000/cookies/get

# Múltiplos cookies
curl -c cookies.txt http://localhost:3000/cookies/set-multiple

# Cookie com Path específico
curl -c cookies.txt "http://localhost:3000/cookies/set?name=path_cookie&value=admin&path=/cookies/path-scoped"
curl -b cookies.txt http://localhost:3000/cookies/path-scoped/check

# Deletar cookie
curl -c cookies.txt "http://localhost:3000/cookies/delete?name=test_cookie"
```

#### Cache

```bash
# ETag
curl -i http://localhost:3000/cache/etag
curl -i http://localhost:3000/cache/etag -H "If-None-Match: \"<etag-value>\""

# Last-Modified
curl -i http://localhost:3000/cache/last-modified
curl -i http://localhost:3000/cache/last-modified -H "If-Modified-Since: Sat, 01 Jan 2026 00:00:00 GMT"

# No-cache
curl -i http://localhost:3000/cache/no-cache

# Max-age customizável
curl -i "http://localhost:3000/cache/max-age?seconds=3600"
```

#### Headers Customizados

```bash
# Receber headers customizados
curl -i http://localhost:3000/headers/custom

# Echo de headers
curl http://localhost:3000/headers/echo -H "X-Custom: value" -H "X-Another: test"
```

#### Content Types

```bash
# JSON
curl http://localhost:3000/content/json

# Texto
curl http://localhost:3000/content/text

# HTML
curl http://localhost:3000/content/html

# Binário
curl "http://localhost:3000/content/binary?size=1024" --output binary.dat
```

#### Autenticação

```bash
# Basic Auth
curl http://localhost:3000/auth/basic -u test:password

# Bearer Token
curl http://localhost:3000/auth/bearer -H "Authorization: Bearer valid-token-123"
```

#### Compression

```bash
# Compressão automática (Bun comprime se Accept-Encoding está presente)
curl http://localhost:3000/compression/auto -H "Accept-Encoding: gzip, br"
```

#### Range Requests

```bash
# Download completo
curl -i http://localhost:3000/range

# Partial content (bytes 0-99)
curl -i http://localhost:3000/range -H "Range: bytes=0-99"
```

#### Large Payloads

```bash
# Download de arquivo grande
curl "http://localhost:3000/large/download?mb=10" --output large.bin

# Upload de arquivo grande
curl -X POST http://localhost:3000/large/upload --data-binary @large.bin
```

### Server-Sent Events (SSE)

```bash
# SSE simples - 10 eventos com intervalo de 1s
curl "http://localhost:3000/sse/simple?count=10&interval=1000"

# Eventos nomeados (message, notification, update, error)
curl http://localhost:3000/sse/named-events

# Stream infinito (heartbeat a cada 5s)
curl http://localhost:3000/sse/infinite

# Eventos multiline
curl http://localhost:3000/sse/multiline

# Simulação de erro
curl http://localhost:3000/sse/error

# Progresso de tarefa (0-100%)
curl http://localhost:3000/sse/progress
```

### WebStreams

```bash
# Stream básico com chunks
curl "http://localhost:3000/stream/basic?chunks=10&chunkSize=1024&interval=100"

# Stream binário
curl "http://localhost:3000/stream/binary?totalBytes=1048576&chunkSize=8192"

# Stream com backpressure
curl http://localhost:3000/stream/backpressure

# Stream NDJSON (newline-delimited JSON)
curl "http://localhost:3000/stream/json?count=100"

# Stream infinito (até cancelar)
curl http://localhost:3000/stream/infinite

# Stream com erro
curl http://localhost:3000/stream/error

# Stream com velocidade variável
curl http://localhost:3000/stream/variable

# Stream de arquivo simulado
curl "http://localhost:3000/stream/file?mb=10" --output streamed.bin

# Stream de linhas de log
curl http://localhost:3000/stream/lines
```

### WebSocket

Para testar WebSocket, use um cliente WebSocket ou navegador:

```javascript
// Echo - ecoa mensagens de volta
const ws = new WebSocket('ws://localhost:3000/ws/echo');
ws.onmessage = (e) => console.log(e.data); // Mensagem inicial inclui cookies do handshake
ws.send('Hello!');

// Broadcast - mensagens para todos os clientes
const ws = new WebSocket('ws://localhost:3000/ws/broadcast');
ws.send(JSON.stringify({ message: 'Hello everyone!' }));

// Room - sistema de salas
const ws = new WebSocket('ws://localhost:3000/ws/room');
ws.send(JSON.stringify({ action: 'join', room: 'chat-room-1' }));
ws.send(JSON.stringify({ action: 'message', message: 'Hello room!' }));

// Binary - enviar/receber dados binários
const ws = new WebSocket('ws://localhost:3000/ws/binary');
ws.binaryType = 'arraybuffer';
ws.send(new Uint8Array([1, 2, 3, 4, 5]));

// Ping/Pong - heartbeat
const ws = new WebSocket('ws://localhost:3000/ws/ping');
ws.onmessage = (e) => {
  const data = JSON.parse(e.data);
  if (data.type === 'ping') {
    ws.send(JSON.stringify({ type: 'pong', timestamp: data.timestamp }));
  }
};

// Stress test - alta frequência de mensagens
const ws = new WebSocket('ws://localhost:3000/ws/stress');
ws.send(JSON.stringify({ action: 'start', count: 1000, interval: 0 }));
```

## 🧪 Testes Automatizados

Para criar testes automatizados, use o exemplo abaixo como base:

```typescript
// test/http.test.ts
import { describe, test, expect } from 'bun:test';

describe('HTTP Routes', () => {
  test('health check returns OK', async () => {
    const response = await fetch('http://localhost:3000/health');
    const text = await response.text();
    expect(text).toBe('OK');
  });

  test('echo returns request info', async () => {
    const response = await fetch('http://localhost:3000/echo');
    const data = await response.json();
    expect(data.method).toBe('GET');
    expect(data.url).toBe('http://localhost:3000/echo');
  });

  test('POST with JSON body', async () => {
    const response = await fetch('http://localhost:3000/methods/post', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ test: 'data' }),
    });
    const data = await response.json();
    expect(data.success).toBe(true);
    expect(data.received.test).toBe('data');
  });
});
```

## 🏗️ Arquitetura

```
server/
├── index.ts              # Servidor principal
├── package.json          # Dependências
├── routes/
│   ├── http.ts          # Rotas HTTP/HTTPS (48 endpoints)
│   ├── sse.ts           # Server-Sent Events (6 endpoints)
│   ├── streams.ts       # WebStreams (9 endpoints)
│   └── websocket.ts     # WebSocket handlers (6 tipos)
└── utils/
    ├── cors.ts          # Helpers CORS
    └── helpers.ts       # Funções auxiliares
```

## 📊 Recursos Implementados

### HTTP/HTTPS
- ✅ Todos os métodos (GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS)
- ✅ Headers customizados
- ✅ Cookies (Set-Cookie, parsing, flags: Secure, HttpOnly, SameSite)
- ✅ Redirects (301, 302, 303, 307, 308)
- ✅ Cache headers (ETag, Last-Modified, Cache-Control, 304)
- ✅ Compression (automática via Bun)
- ✅ Content types (JSON, text, HTML, binary)
- ✅ Status codes variados
- ✅ Authentication (Basic, Bearer)
- ✅ Range requests (206 Partial Content)
- ✅ CORS completo
- ✅ Large payloads (upload/download)

### WebSocket
- ✅ Echo (texto e binário)
- ✅ Broadcast / Pub-Sub
- ✅ Room-based messaging
- ✅ Binary messages
- ✅ Ping/Pong heartbeat
- ✅ Backpressure handling
- ✅ Compression (per-message deflate)
- ✅ Stress testing

### Server-Sent Events (SSE)
- ✅ Eventos simples
- ✅ Eventos nomeados
- ✅ IDs de eventos
- ✅ Reconexão automática (retry)
- ✅ Dados multiline
- ✅ Stream infinito
- ✅ Progress tracking

### WebStreams
- ✅ ReadableStream com chunks
- ✅ Backpressure
- ✅ Binary streams
- ✅ JSON streams (NDJSON)
- ✅ Cancelamento
- ✅ Error handling
- ✅ Variable speed
- ✅ Large file streaming

## 🔧 Configuração

### Variáveis de Ambiente

```bash
# Porta (padrão: 3000)
PORT=3000

# Host (padrão: 0.0.0.0)
HOST=0.0.0.0

# Modo de desenvolvimento
NODE_ENV=development
```

### HTTPS (Opcional)

Para habilitar HTTPS, descomente e configure no [index.ts](index.ts:52-55):

```typescript
tls: {
  cert: Bun.file("./certs/cert.pem"),
  key: Bun.file("./certs/key.pem"),
}
```

## 📝 Notas

- O servidor usa Bun v1.3.8+
- Todas as rotas suportam CORS
- WebSocket suporta compression automática
- SSE envia retry automático
- Streams suportam backpressure nativo
- Logs de requisições no console

## 🐛 Troubleshooting

### Porta já em uso

```bash
# Encontrar processo usando a porta 3000
lsof -ti:3000

# Matar o processo
kill -9 $(lsof -ti:3000)
```

### Erro de tipos TypeScript

```bash
# Reinstalar dependências
rm -rf node_modules bun.lockb
bun install
```

## 📚 Referências

- [Bun Documentation](https://bun.sh/docs)
- [W3C Fetch API](https://fetch.spec.whatwg.org/)
- [W3C WebSocket API](https://websockets.spec.whatwg.org/)
- [W3C Server-Sent Events](https://html.spec.whatwg.org/multipage/server-sent-events.html)
- [WHATWG Streams API](https://streams.spec.whatwg.org/)

---

**Data de criação:** 2026-02-15
**Versão:** 1.0.0
**Maintainer:** @meedwire/browser-network
