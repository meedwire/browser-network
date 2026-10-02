# browser-network

Núcleo de rede em C++ (HTTP, cookies, cache, CORS, redirects, streams, SSE e WebSocket) com integração para React Native via JSI.

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
