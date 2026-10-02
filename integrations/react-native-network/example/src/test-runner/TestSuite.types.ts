/**
 * @file TestSuite.types.ts
 * @brief Tipos e interfaces do framework de testes
 *
 * @date 2026-02-16
 */

/**
 * Status possíveis de um teste
 */
export type TestStatus = 'pending' | 'running' | 'passed' | 'failed';

/**
 * Representa um teste individual
 */
export interface Test {
  /** Identificador único do teste */
  id: string;
  /** Nome completo do teste (suite > nome) */
  name: string;
  /** Nome da suite à qual pertence */
  suite: string;
  /** Status atual do teste */
  status: TestStatus;
  /** Mensagem de erro (se falhou) */
  error?: string;
  /** Função de teste a ser executada */
  fn: () => Promise<void> | void;
}

/**
 * Callback para definição de testes dentro de uma suite
 */
export type ItCallback = (name: string, fn: () => Promise<void> | void) => void;

/**
 * API passada para o callback do describe
 */
export interface DescribeAPI {
  it: ItCallback;
}

/**
 * Callback para definição de uma suite de testes
 */
export type DescribeCallback = (api: DescribeAPI) => void;

/**
 * API pública do TestSuite
 */
export interface TestSuiteAPI {
  /** Registra uma suite de testes */
  describe: (name: string, callback: DescribeCallback) => void;
  /** Mapa de testes registrados */
  tests: Map<string, Test>;
  /** Executa um teste específico por ID */
  run: (testId: string) => Promise<void>;
  /** Executa todos os testes de uma suite */
  runSuite: (suiteName: string) => Promise<void>;
  /** Executa todos os testes registrados */
  runAll: () => Promise<void>;
  /** Retorna array de todos os testes */
  getTests: () => Test[];
  /** Retorna array de nomes de suites únicas */
  getSuites: () => string[];
  /** Reseta o estado de todos os testes */
  reset: () => void;
}

/**
 * Interface de matchers para assertions
 */
export interface ExpectMatchers {
  /** Verifica igualdade estrita (===) */
  toBe: (expected: any) => void;
  /** Verifica se o valor é truthy */
  toBeTruthy: () => void;
  /** Verifica se o valor é falsy */
  toBeFalsy: () => void;
  /** Verifica igualdade profunda (JSON) */
  toEqual: (expected: any) => void;
  /** Verifica se string contém substring */
  toContain: (expected: string) => void;
  /** Verifica se número é maior que */
  toBeGreaterThan: (expected: number) => void;
}
