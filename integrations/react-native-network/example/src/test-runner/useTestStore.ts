/**
 * @file useTestStore.ts
 * @brief Store Zustand para gerenciamento reativo do estado dos testes
 *
 * @date 2026-02-16
 */

import { create } from 'zustand';
import type { Test, TestStatus } from './TestSuite.types';

interface TestStore {
  /** Mapa de testes registrados, indexados por ID */
  tests: Record<string, Test>;
  /** Indica se uma bateria de testes está em execução */
  isRunning: boolean;

  /** Registra um novo teste na store */
  registerTest: (test: Test) => void;
  /** Atualiza o status e erro de um teste específico */
  updateTestStatus: (id: string, status: TestStatus, error?: string) => void;
  /** Define se os testes estão rodando */
  setIsRunning: (running: boolean) => void;
  /** Reseta o estado de todos os testes para 'pending' */
  resetTests: () => void;
  /** Limpa todos os testes registrados */
  clearTests: () => void;
}

export const useTestStore = create<TestStore>((set) => ({
  tests: {},
  isRunning: false,

  registerTest: (test) =>
    set((state) => ({
      tests: { ...state.tests, [test.id]: test },
    })),

  updateTestStatus: (id, status, error) =>
    set((state) => {
      const test = state.tests[id];
      if (!test) return state;
      return {
        tests: {
          ...state.tests,
          [id]: { ...test, status, error },
        },
      };
    }),

  setIsRunning: (running) => set({ isRunning: running }),

  resetTests: () =>
    set((state) => ({
      tests: Object.fromEntries(
        Object.entries(state.tests).map(([id, test]) => [
          id,
          { ...test, status: 'pending', error: undefined },
        ])
      ),
    })),

  clearTests: () => set({ tests: {} }),
}));
