/**
 * @file expect.ts
 * @brief Matchers para assertions em testes
 *
 * Fornece uma API similar ao Jest para validação de valores.
 *
 * @date 2026-02-16
 */

import type { ExpectMatchers } from './TestSuite.types';

/**
 * Cria um objeto com matchers para assertion
 *
 * @param actual Valor a ser testado
 * @returns Objeto com métodos de assertion
 *
 * @example
 * ```typescript
 * expect(1 + 1).toBe(2);
 * expect(response.ok).toBeTruthy();
 * expect(data).toEqual({ foo: 'bar' });
 * expect(text).toContain('hello');
 * expect(count).toBeGreaterThan(0);
 * ```
 */
export const expect = (actual: any): ExpectMatchers => ({
  /**
   * Verifica igualdade estrita (===)
   *
   * @param expected Valor esperado
   * @throws Error se os valores não forem iguais
   */
  toBe: (expected: any): void => {
    if (actual !== expected) {
      throw new Error(`Expected ${expected}, but got ${actual}`);
    }
  },

  /**
   * Verifica se o valor é truthy
   *
   * @throws Error se o valor for falsy
   */
  toBeTruthy: (): void => {
    if (!actual) {
      throw new Error(`Expected truthy, but got ${actual}`);
    }
  },

  /**
   * Verifica se o valor é falsy
   *
   * @throws Error se o valor for truthy
   */
  toBeFalsy: (): void => {
    if (actual) {
      throw new Error(`Expected falsy, but got ${actual}`);
    }
  },

  /**
   * Verifica igualdade profunda usando JSON
   *
   * @param expected Valor esperado
   * @throws Error se os valores não forem iguais
   */
  toEqual: (expected: any): void => {
    const a = JSON.stringify(actual);
    const e = JSON.stringify(expected);
    if (a !== e) {
      throw new Error(`Expected ${e}, but got ${a}`);
    }
  },

  /**
   * Verifica se string contém substring
   *
   * @param expected Substring esperada
   * @throws Error se não contiver a substring
   */
  toContain: (expected: string): void => {
    if (!String(actual).includes(expected)) {
      throw new Error(`Expected "${actual}" to contain "${expected}"`);
    }
  },

  /**
   * Verifica se número é maior que o esperado
   *
   * @param expected Valor de referência
   * @throws Error se não for maior
   */
  toBeGreaterThan: (expected: number): void => {
    if (actual <= expected) {
      throw new Error(`Expected ${actual} to be greater than ${expected}`);
    }
  },
});
