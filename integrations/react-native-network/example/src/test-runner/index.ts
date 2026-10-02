/**
 * @file index.ts
 * @brief Ponto de entrada do framework de testes
 *
 * Exporta todos os componentes necessários para criar e executar testes.
 *
 * @date 2026-02-16
 */

// Classe principal
export { TestSuite } from './TestSuite';

// Matchers para assertions
export { expect } from './expect';

// Store Zustand
export { useTestStore } from './useTestStore';

// Tipos e interfaces
export type {
  Test,
  TestStatus,
  TestSuiteAPI,
  DescribeAPI,
  DescribeCallback,
  ItCallback,
  ExpectMatchers,
} from './TestSuite.types';
