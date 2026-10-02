#pragma once

#include <jsi/jsi.h>
#include <ReactCommon/CallInvoker.h>
#include <memory>

/**
 * @file WebSocketJSI.h
 * @brief Módulo JSI para integração de WebSockets no React Native.
 *
 * Expõe as funcionalidades de conexão, envio e fechamento de WebSockets
 * para o ambiente JavaScript através da JSI (JavaScript Interface).
 * Utiliza o WebSocketAdapter do Core C++ para a implementação real.
 */

namespace browsernetwork {

using namespace facebook;

/**
 * @brief Instala os métodos do módulo WebSocket no objeto global ou módulo fornecido.
 *
 * @param jsiRuntime Runtime JSI do React Native.
 * @param browserNetworkModule Objeto módulo onde as funções serão instaladas.
 * @param jsCallInvoker Invoker para chamar funções JS a partir de threads nativas.
 */
void installWebSocket(jsi::Runtime &jsiRuntime, jsi::Object &browserNetworkModule, std::shared_ptr<react::CallInvoker> jsCallInvoker);

} // namespace browsernetwork
