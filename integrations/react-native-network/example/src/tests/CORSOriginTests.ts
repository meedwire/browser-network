import { TestSuite } from '../test-runner';

export const CORSOriginTests = (suite: TestSuite) => {
  const { describe } = suite.getAPI();
  const BASE_URL = 'http://10.0.2.2:3000';

  describe('CORS & Origin', ({ it }) => {
    it('should send the correct synthetic origin and be accepted', async () => {
      const response = await fetch(`${BASE_URL}/cors/restricted`);
      const data = await response.json();

      if (response.status !== 200) {
        throw new Error(
          `Expected status 200, got ${response.status}. Message: ${data.message}`
        );
      }

      if (!data.success) {
        throw new Error(
          `Expected success: true, got false. Message: ${data.message}`
        );
      }

      if (data.origin !== 'https://network-example.app') {
        throw new Error(
          `Expected origin https://network-example.app, got ${data.origin}`
        );
      }
    });

    it('should echo the origin in a standard echo request', async () => {
      const response = await fetch(`${BASE_URL}/echo`);
      const data = await response.json();

      const receivedOrigin = data.headers['origin'] || data.headers['Origin'];

      if (receivedOrigin !== 'https://network-example.app') {
        throw new Error(
          `Expected origin https://network-example.app, got ${receivedOrigin}`
        );
      }
    });
  });
};
