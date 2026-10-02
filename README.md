# browser-network

Rede com comportamento de navegador para React Native: `fetch`, `WebSocket`, `EventSource` e streams sobre um núcleo C++ (libcurl + IXWebSocket), exposto via JSI.

## Paridade com o navegador

Legenda: ✅ igual ao navegador · ⚠️ parcial · 🔲 ainda não suportado

Status levantado a partir do código atual. A contagem de testes vem de [docs/FEATURES.md](docs/FEATURES.md) (189 testes: 20 em C++ e 169 no app de exemplo).

### fetch / Request / Response

| Recurso | Status | Observação |
| --- | --- | --- |
| Métodos (`GET`, `POST`, `PUT`, `PATCH`, `DELETE`, `HEAD`, `OPTIONS`) | ✅ | |
| `Headers` (case-insensitive, múltiplos valores) | ✅ | |
| Body string, `URLSearchParams`, `ArrayBuffer`, `TypedArray` | ✅ | |
| Body `FormData` | ⚠️ | Só campos de texto; arquivos/`Blob` lançam `TypeError` |
| Body `Blob` / `ReadableStream` (upload em streaming) | 🔲 | |
| `response.text()`, `json()`, `arrayBuffer()`, `bodyUsed` | ✅ | |
| `response.blob()`, `formData()`, `clone()` | 🔲 | |
| `response.body` (`ReadableStream` + `getReader()`) | ✅ | Com backpressure e cancelamento |
| `tee()`, `pipeTo()`, `pipeThrough()` | 🔲 | |
| `redirect`: `follow` / `error` / `manual`, `redirected`, `url` | ✅ | Regras 301/302/303/307/308 e remoção de headers sensíveis cross-origin |
| `AbortController` / `signal` | ✅ | |
| `credentials` (`omit` / `same-origin` / `include`) | ✅ | |
| `cache` (modo da requisição: `no-store`, `reload`...) | 🔲 | O cache HTTP funciona, mas o modo por requisição não é lido |
| `XMLHttpRequest` | 🔲 | Fica o XHR nativo do React Native |

### Semântica de navegador

| Recurso | Status | Observação |
| --- | --- | --- |
| Cache HTTP (RFC 7234 / 5861) | ✅ | `max-age`, `no-cache`, `Vary`, ETag/304, `stale-while-revalidate`, `stale-if-error` |
| Cache em disco | 🔲 | Só em memória (LRU, 100 entradas) |
| Cookies (`Secure`, `SameSite`, Public Suffix List) | ✅ | Cookie jar próprio |
| Persistência de cookies em disco | ✅ | iOS e Android, arquivo no diretório do app |
| Origin sintética e Secure Context | ✅ | `com.empresa.app` → `https://com-empresa-app.app` |
| CORS, preflight automático e cache de preflight | ✅ | |
| Descompressão automática (gzip/brotli) | 🔲 | |
| Pool de conexões / multiplexação HTTP/2 | 🔲 | Um handle libcurl por requisição |
| Cache de DNS compartilhado | 🔲 | |
| Proxy e certificate pinning | 🔲 | |

### WebSocket

| Recurso | Status | Observação |
| --- | --- | --- |
| `ws://` e `wss://`, subprotocolos | ✅ | |
| `send()` texto, `ArrayBuffer` e `TypedArray` | ✅ | |
| `send(Blob)` | 🔲 | Lança `TypeError` |
| `binaryType` (`blob` / `arraybuffer`), `bufferedAmount` | ✅ | |
| Eventos e `addEventListener` | ✅ | |

### EventSource (SSE)

| Recurso | Status | Observação |
| --- | --- | --- |
| `onmessage`, eventos nomeados, dados multilinha | ✅ | |
| `lastEventId` / `Last-Event-ID`, `retry` e reconexão | ✅ | |
| `withCredentials`, `readyState` | ✅ | |

### Depuração

| Recurso | Status | Observação |
| --- | --- | --- |
| Aba Network do Chrome DevTools (Hermes) | ✅ | Headers, body e timing |
| Body de respostas em streaming no DevTools | ⚠️ | Aparece só ao fim da stream |

## Estrutura

| Pasta | Conteúdo |
| --- | --- |
| `cpp/` | Núcleo C++ (`core`, `ports`, `adapters`) e testes (`cpp/tests`) |
| `integrations/react-native-network/` | Biblioteca React Native (iOS/Android) e app de exemplo |
| `server/` | Servidor de testes em Bun (HTTP, SSE, WebStreams, WebSocket) |
| `scripts/` | Build do núcleo para iOS (`xcframework`) e Android |
| `docs/` | Arquitetura, decisões e cobertura de testes |

## Começando

### Testes C++

```bash
cd cpp
mkdir -p build && cd build
cmake -DBUILD_TESTS=ON ..
make
ctest
```

### Servidor de testes

```bash
cd server
bun install
bun run start
```

### React Native

O `xcframework` do iOS não é versionado; gere antes de rodar o exemplo:

```bash
./scripts/build_ios_core.sh
./scripts/build_android_core.sh
```

Depois, em `integrations/react-native-network/example`, instale as dependências e rode `yarn ios` ou `yarn android`.

## Documentação

- [Arquitetura](docs/ARCHITECTURE.md)
- [Funcionalidades](docs/FEATURES.md)
- [Decisões](docs/DECISIONS.md) · [Log de decisões](docs/DECISION_LOG.md)
- [Cobertura de testes](docs/TEST_COVERAGE_REPORT.md)
- [Servidor de testes](server/README.md)
