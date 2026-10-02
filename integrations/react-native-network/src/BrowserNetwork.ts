import NativeNetwork from './NativeNetwork';

export interface NetworkEvent {
  type: 'beforeRequest' | 'responseReceived' | 'error';
  requestId: string;
  url?: string;
  method?: string;
  headers?: Record<string, string>;
  status?: number;
  body?: string;
  message?: string;
}

export type NetworkObserver = (event: NetworkEvent) => void;

/**
 * BrowserNetwork API for React Native
 */
export const BrowserNetwork = {
  /**
   * Instala o polyfill e inicializa o stack C++.
   * @param bundleId ID do bundle para segurança (CORS/Origin).
   */
  install(bundleId: string): boolean {
    return NativeNetwork.install(bundleId);
  },

  /**
   * Define um observador customizado para eventos de rede.
   * @param callback Função que será chamada para cada evento de rede.
   */
  setNetworkObserver(callback: NetworkObserver): void {
    if (NativeNetwork && 'setNetworkObserver' in NativeNetwork) {
      (NativeNetwork as any).setNetworkObserver(callback);
    }
  },
};
