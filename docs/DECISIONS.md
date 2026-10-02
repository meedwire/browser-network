# Registro de Decisões Técnicas

## [15/02/2026] - Reestruturação da Integração React Native

### Decisão: Reconstrução do Scaffold do Turbo Module

- **Contexto**: O diretório `integrations/react-native-turbo` passou por muitas modificações experimentais durante a fase de debug da pré-compilação.
- **Ação**: Deletar o diretório existente e gerar um novo usando `create-react-native-library`.
- **Racional**: Garantir um ambiente limpo, seguindo os padrões atuais da comunidade e facilitando a manutenção futura com Nitro Modules. O uso de `bun` foi solicitado explicitamente para consistência.
- **Ferramentas**: `create-react-native-library` (tipo: Turbo Module), `bun` para gerenciamento de dependências.

### Decisão: Estratégia de Linkage de Binários Pré-compilados

- **Contexto**: A compilação do core C++ a partir do código fonte (source-build) dentro do build do Android/iOS provou ser instável e lenta.
- **Ação**: Utilizar binários `.so` (Android) e `.xcframework` (iOS) gerados via scripts dedicados.
- **Racional**: Isolamento de erros, build mais rápido para o consumidor da biblioteca e estabilidade entre diferentes versões do React Native.

### Decisão: Otimização de Build C++ e Linkage Estático (Android)

- **Contexto**: O build estava demorando excessivamente e ocorriam erros de bibliotecas não encontradas (`UnsatisfiedLinkError`).
- **Ação**: Implementado paralelismo manual via `sysctl` e linkage estático forçado de dependências (Abseil/Curl) no Android.
- **Racional**: Reduzir tempo de build de horas para minutos e garantir que a biblioteca seja self-contained, resolvendo problemas de dependências externas no runtime.

## [15/02/2026] - Implementação de Polyfills e Infraestrutura de Teste (Tarefa 1)

### Decisão: Servidor de Teste Spec-Compliant com Bun

- **Contexto**: Validar 100% dos recursos de navegador requer um backend que suporte WebSockets, SSE e WebStreams de forma nativa e rápida.
- **Ação**: Criar um servidor centralizado em `/server` usando `Bun.serve()`.
- **Racional**: Bun oferece a melhor performance e suporte nativo a padrões modernos de web, ideal para testar a conformidade da nossa biblioteca com as especificações W3C.

### Decisão: Modelo de Teste On-Device (Estilo opsqlite)

- **Contexto**: A integração JSI precisa ser testada no ambiente real (Android/iOS) para validar threads, memória e callbacks.
- **Ação**: Implementar uma suíte de testes interna no app de exemplo com interface visual para feedback imediato de `passed/failed`.
- **Racional**: Este modelo (popularizado por bibliotecas como `op-sqlite`) é a forma mais confiável de garantir que a ponte nativa se comporte como esperado, evitando "falsos positivos" de testes unitários apenas no JS.

### Decisão: Implementação de Polyfills Globais (`fetch`, `Headers`, `Response`)

- **Contexto**: O usuário deseja substituir o `fetch` padrão por uma versão de alta performance baseada no C++ Core.
- **Ação**: Criar um objeto proxy `__BrowserNetworkProxy` (JSI) e expor funções globais via `setup()`.
- **Racional**: Permitir que desenvolvedores usem a biblioteca sem mudar seu código existente, seguindo 100% o contrato das APIs WHATWG.
