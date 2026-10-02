/**
 * @file TestSuite.ts
 * @brief Framework de testes com registro e execução separados
 *
 * Permite registrar testes via describe/it e executá-los posteriormente,
 * com acesso à lista completa de testes antes da execução.
 *
 * @date 2026-02-16
 */

import type {
  Test,
  TestSuiteAPI,
  DescribeCallback,
  ItCallback,
} from './TestSuite.types';
import { useTestStore } from './useTestStore';

/**
 * @class TestSuite
 * @brief Gerencia registro e execução de testes
 *
 * Exemplo de uso:
 * ```typescript
 * const suite = new TestSuite('my-tests');
 * const { describe } = suite.getAPI();
 *
 * describe('Math', ({ it }) => {
 *   it('should add numbers', () => {
 *     expect(1 + 1).toBe(2);
 *   });
 * });
 *
 * await suite.runAll();
 * ```
 */
export class TestSuite {
  private tests: Map<string, Test> = new Map();
  private currentSuite = '';
  private testCounter = 0;
  private prefix: string;

  /**
   * Cria uma nova instância de TestSuite
   *
   * @param prefix Prefixo para IDs dos testes (ex: 'basic', 'advanced')
   */
  constructor(prefix: string = '') {
    this.prefix = prefix;
  }

  /**
   * Registra uma suite de testes
   *
   * @param name Nome da suite
   * @param callback Função que define os testes da suite
   */
  describe = (name: string, callback: DescribeCallback): void => {
    this.currentSuite = name;
    callback({ it: this.it });
    this.currentSuite = '';
  };

  /**
   * Registra um teste individual dentro da suite atual
   *
   * @param name Nome do teste
   * @param fn Função de teste (pode ser async)
   */
  it: ItCallback = (name: string, fn: () => Promise<void> | void): void => {
    const id = this.prefix
      ? `${this.prefix}_test_${this.testCounter++}`
      : `test_${this.testCounter++}`;
    const fullName = this.currentSuite
      ? `${this.currentSuite} > ${name}`
      : name;

    const test: Test = {
      id,
      name: fullName,
      suite: this.currentSuite,
      status: 'pending',
      fn,
    };

    this.tests.set(id, test);
    useTestStore.getState().registerTest(test);
  };

  /**
   * Executa um teste específico por ID
   *
   * @param testId ID do teste a executar
   * @throws Error se o teste não for encontrado
   */
  run = async (testId: string): Promise<void> => {
    const test = this.tests.get(testId);
    if (!test) {
      throw new Error(`Test ${testId} not found`);
    }

    test.status = 'running';
    useTestStore.getState().updateTestStatus(testId, 'running');

    try {
      await test.fn();
      test.status = 'passed';
      delete test.error;
      useTestStore.getState().updateTestStatus(testId, 'passed');
    } catch (e: any) {
      test.status = 'failed';
      test.error = e.message;
      useTestStore.getState().updateTestStatus(testId, 'failed', e.message);
    }
  };

  /**
   * Executa todos os testes de uma suite específica
   *
   * @param suiteName Nome da suite
   */
  runSuite = async (suiteName: string): Promise<void> => {
    const suiteTests = Array.from(this.tests.values()).filter(
      (t) => t.suite === suiteName
    );

    for (const test of suiteTests) {
      await this.run(test.id);
    }
  };

  /**
   * Executa todos os testes registrados
   */
  runAll = async (): Promise<void> => {
    for (const [id] of this.tests) {
      await this.run(id);
    }
  };

  /**
   * Retorna array de todos os testes
   */
  getTests = (): Test[] => {
    return Array.from(this.tests.values());
  };

  /**
   * Retorna array de nomes de suites únicas
   */
  getSuites = (): string[] => {
    const suites = new Set<string>();
    this.tests.forEach((test) => {
      if (test.suite) suites.add(test.suite);
    });
    return Array.from(suites);
  };

  /**
   * Reseta o estado de todos os testes para 'pending'
   */
  reset = (): void => {
    this.tests.forEach((test) => {
      test.status = 'pending';
      delete test.error;
    });
    useTestStore.getState().resetTests();
  };

  /**
   * Retorna a API pública do TestSuite
   *
   * @returns Objeto com métodos públicos da suite
   */
  getAPI = (): TestSuiteAPI => ({
    describe: this.describe,
    tests: this.tests,
    run: this.run,
    runSuite: this.runSuite,
    runAll: this.runAll,
    getTests: this.getTests,
    getSuites: this.getSuites,
    reset: this.reset,
  });
}
