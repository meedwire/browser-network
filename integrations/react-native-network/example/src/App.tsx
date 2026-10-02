import { useState, useMemo, useRef, useEffect } from 'react';
import {
  StyleSheet,
  Text,
  View,
  ScrollView,
  TouchableOpacity,
} from 'react-native';
import * as Network from '@meedwire/browser-network';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import { createBasicTests } from './tests/BasicTests';
import { createAdvancedTests } from './tests/AdvancedTests';
import { SSETests } from './tests/SSETests';
import { WebSocketTests } from './tests/WebSocketTests';
import { StreamTests } from './tests/StreamTests';
import { RedirectTests } from './tests/RedirectTests';
import { CORSOriginTests } from './tests/CORSOriginTests';
import { useTestStore, TestSuite } from './test-runner';

// Inicializa os polyfills (fetch, Headers, Response, EventSource)
Network.setup();

type TestFilter =
  | 'all'
  | 'basic'
  | 'advanced'
  | 'sse'
  | 'websocket'
  | 'streams'
  | 'redirects'
  | 'cors';

interface FilterButtonProps {
  active: boolean;
  label: string;
  onPress: () => void;
}

const FilterButton = ({ active, label, onPress }: FilterButtonProps) => (
  <TouchableOpacity
    style={[styles.filterButton, active && styles.filterButtonActive]}
    onPress={onPress}
  >
    <Text
      style={[styles.filterButtonText, active && styles.filterButtonTextActive]}
    >
      {label}
    </Text>
  </TouchableOpacity>
);

export default function App() {
  const tests = useTestStore((state) => state.tests);
  const isRunning = useTestStore((state) => state.isRunning);
  const setIsRunning = useTestStore((state) => state.setIsRunning);

  const [filter, setFilter] = useState<TestFilter>('all');
  const scrollViewRef = useRef<any>(null);
  const testPositions = useRef<Map<string, number>>(new Map());

  // Inicializar test suites uma vez
  const basicSuite = useMemo(() => createBasicTests(), []);
  const advancedSuite = useMemo(() => createAdvancedTests(), []);
  const sseSuite = useMemo(() => {
    const suite = new TestSuite('sse');
    SSETests(suite);
    return suite;
  }, []);

  const webSocketSuite = useMemo(() => {
    const suite = new TestSuite('websocket');
    WebSocketTests(suite);
    return suite;
  }, []);

  const streamSuite = useMemo(() => {
    const suite = new TestSuite('streams');
    StreamTests(suite);
    return suite;
  }, []);

  const redirectSuite = useMemo(() => {
    const suite = new TestSuite('redirects');
    RedirectTests(suite);
    return suite;
  }, []);

  const corsSuite = useMemo(() => {
    const suite = new TestSuite('cors');
    CORSOriginTests(suite);
    return suite;
  }, []);

  // Combinar todos os testes do store
  const allTests = useMemo(
    () => [
      ...basicSuite.getTests(),
      ...advancedSuite.getTests(),
      ...sseSuite.getTests(),
      ...webSocketSuite.getTests(),
      ...streamSuite.getTests(),
      ...redirectSuite.getTests(),
      ...corsSuite.getTests(),
    ],
    [
      tests,
      basicSuite,
      advancedSuite,
      sseSuite,
      webSocketSuite,
      streamSuite,
      redirectSuite,
      corsSuite,
    ]
  );

  // Filtrar testes baseado na seleção
  const filteredTests = useMemo(() => {
    if (filter === 'all') return allTests;
    return allTests.filter((t) => t.id.startsWith(filter));
  }, [filter, allTests]);

  const runTests = async () => {
    setIsRunning(true);

    try {
      if (filter === 'basic' || filter === 'all') {
        await basicSuite.runAll();
      }
      if (filter === 'advanced' || filter === 'all') {
        await advancedSuite.runAll();
      }
      if (filter === 'sse' || filter === 'all') {
        await sseSuite.runAll();
      }
      if (filter === 'websocket' || filter === 'all') {
        await webSocketSuite.runAll();
      }
      if (filter === 'streams' || filter === 'all') {
        await streamSuite.runAll();
      }
      if (filter === 'redirects' || filter === 'all') {
        await redirectSuite.runAll();
      }
      if (filter === 'cors' || filter === 'all') {
        await corsSuite.runAll();
      }
    } catch (error) {
      console.error('[App] Error running tests:', error);
    } finally {
      setIsRunning(false);
    }
  };

  const runSingleTest = async (testId: string) => {
    setIsRunning(true);

    try {
      // Determinar qual suite contém o teste
      const test = tests[testId];
      if (!test) return;

      let suite;
      if (testId.startsWith('basic')) {
        suite = basicSuite;
      } else if (testId.startsWith('advanced')) {
        suite = advancedSuite;
      } else if (testId.startsWith('sse')) {
        suite = sseSuite;
      } else if (testId.startsWith('websocket')) {
        suite = webSocketSuite;
      } else if (testId.startsWith('streams')) {
        suite = streamSuite;
      } else if (testId.startsWith('cors')) {
        suite = corsSuite;
      } else {
        suite = redirectSuite;
      }

      await suite.run(testId);
    } catch (error) {
      console.error('[App] Error running test:', error);
    } finally {
      setIsRunning(false);
    }
  };

  const resetTests = () => {
    basicSuite.reset();
    advancedSuite.reset();
    sseSuite.reset();
    webSocketSuite.reset();
    streamSuite.reset();
    redirectSuite.reset();
    corsSuite.reset();
  };

  // Auto-scroll para o teste que está executando
  useEffect(() => {
    const runningTest = filteredTests.find((t) => t.status === 'running');
    if (runningTest && scrollViewRef.current) {
      const position = testPositions.current.get(runningTest.id);
      if (position !== undefined) {
        scrollViewRef.current.scrollTo({
          y: Math.max(0, position - 100), // Offset de 100px para não ficar no topo
          animated: true,
        });
      }
    }
  }, [filteredTests]);

  const passedCount = allTests.filter((t) => t.status === 'passed').length;
  const failedCount = allTests.filter((t) => t.status === 'failed').length;
  const pendingCount = allTests.filter((t) => t.status === 'pending').length;

  const basicCount = allTests.filter((t) => t.id.startsWith('basic')).length;
  const advancedCount = allTests.filter((t) =>
    t.id.startsWith('advanced')
  ).length;
  const sseCount = allTests.filter((t) => t.id.startsWith('sse')).length;
  const webSocketCount = allTests.filter((t) =>
    t.id.startsWith('websocket')
  ).length;
  const streamCount = allTests.filter((t) => t.id.startsWith('streams')).length;
  const redirectCount = allTests.filter((t) =>
    t.id.startsWith('redirects')
  ).length;
  const corsCount = allTests.filter((t) => t.id.startsWith('cors')).length;
  const totalCount = allTests.length;

  //Dont remove this log
  console.log(
    'Failed tests:',
    allTests.filter((t) => t.status === 'failed')
  );

  return (
    <SafeAreaProvider>
      <SafeAreaView style={styles.container}>
        <View style={styles.header}>
          <Text style={styles.title}>Browser Network Tests 🧪</Text>
          <Text style={styles.subtitle}>
            {isRunning
              ? 'Rodando testes...'
              : `${passedCount} ✅ • ${failedCount} ❌ • ${pendingCount} ⏳`}
          </Text>

          {!isRunning && (
            <>
              <View style={styles.filterSelector}>
                <FilterButton
                  active={filter === 'all'}
                  label={`Todos (${totalCount})`}
                  onPress={() => setFilter('all')}
                />
                <FilterButton
                  active={filter === 'basic'}
                  label={`Básicos (${basicCount})`}
                  onPress={() => setFilter('basic')}
                />
                <FilterButton
                  active={filter === 'advanced'}
                  label={`Avançados (${advancedCount})`}
                  onPress={() => setFilter('advanced')}
                />
                <FilterButton
                  active={filter === 'sse'}
                  label={`SSE (${sseCount})`}
                  onPress={() => setFilter('sse')}
                />
                <FilterButton
                  active={filter === 'websocket'}
                  label={`WebSocket (${webSocketCount})`}
                  onPress={() => setFilter('websocket')}
                />
                <FilterButton
                  active={filter === 'streams'}
                  label={`Streams (${streamCount})`}
                  onPress={() => setFilter('streams')}
                />
                <FilterButton
                  active={filter === 'redirects'}
                  label={`Redirects (${redirectCount})`}
                  onPress={() => setFilter('redirects')}
                />
                <FilterButton
                  active={filter === 'cors'}
                  label={`CORS/Origin (${corsCount})`}
                  onPress={() => setFilter('cors')}
                />
              </View>

              <View style={styles.actionButtons}>
                <TouchableOpacity
                  style={[styles.button, styles.runButton]}
                  onPress={runTests}
                >
                  <Text style={styles.buttonText}>▶ EXECUTAR</Text>
                </TouchableOpacity>

                <TouchableOpacity
                  style={[styles.button, styles.resetButton]}
                  onPress={resetTests}
                >
                  <Text style={styles.buttonText}>⟳ LIMPAR</Text>
                </TouchableOpacity>
              </View>
            </>
          )}
        </View>

        <ScrollView ref={scrollViewRef} style={styles.logContainer}>
          {filteredTests.map((test) => (
            <View
              key={test.id}
              onLayout={(event) => {
                const { y } = event.nativeEvent.layout;
                testPositions.current.set(test.id, y);
              }}
            >
              <TouchableOpacity
                style={styles.resultItem}
                onPress={() => !isRunning && runSingleTest(test.id)}
                disabled={isRunning}
              >
                <Text
                  style={[
                    styles.statusDot,
                    test.status === 'passed' && { color: '#44ff44' },
                    test.status === 'failed' && { color: '#ff4444' },
                    test.status === 'running' && { color: '#ffaa44' },
                    test.status === 'pending' && { color: '#888' },
                  ]}
                >
                  {test.status === 'passed' && '✓'}
                  {test.status === 'failed' && '✗'}
                  {test.status === 'running' && '⟳'}
                  {test.status === 'pending' && '⏳'}
                </Text>
                <View style={styles.resultTextContainer}>
                  <Text style={styles.testName}>{test.name}</Text>
                  {test.error && (
                    <Text style={styles.errorText}>{test.error}</Text>
                  )}
                </View>
              </TouchableOpacity>
            </View>
          ))}
        </ScrollView>
      </SafeAreaView>
    </SafeAreaProvider>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0a0a0a',
  },
  header: {
    padding: 20,
    backgroundColor: '#1a1a1a',
    borderBottomWidth: 1,
    borderBottomColor: '#333',
  },
  title: {
    fontSize: 22,
    fontWeight: 'bold',
    color: '#fff',
  },
  subtitle: {
    marginTop: 5,
    fontSize: 14,
    color: '#aaa',
  },
  filterSelector: {
    flexDirection: 'row',
    marginTop: 15,
    flexWrap: 'wrap',
    gap: 8,
  },
  filterButton: {
    flex: 1,
    padding: 10,
    borderRadius: 8,
    alignItems: 'center',
    backgroundColor: '#2a2a2a',
    borderWidth: 2,
    borderColor: '#3a3a3a',
    minWidth: 100,
  },
  filterButtonActive: {
    backgroundColor: '#1a4d7a',
    borderColor: '#007AFF',
  },
  filterButtonText: {
    color: '#999',
    fontSize: 12,
    fontWeight: '600',
  },
  filterButtonTextActive: {
    color: '#fff',
  },
  actionButtons: {
    flexDirection: 'row',
    marginTop: 15,
    gap: 8,
  },
  button: {
    flex: 1,
    padding: 12,
    borderRadius: 8,
    alignItems: 'center',
  },
  runButton: {
    backgroundColor: '#007AFF',
  },
  resetButton: {
    backgroundColor: '#555',
  },
  buttonText: {
    color: '#fff',
    fontWeight: 'bold',
    fontSize: 14,
  },
  logContainer: {
    flex: 1,
    padding: 10,
  },
  resultItem: {
    flexDirection: 'row',
    padding: 12,
    borderBottomWidth: 1,
    borderBottomColor: '#222',
  },
  statusDot: {
    fontSize: 18,
    marginRight: 10,
    fontWeight: 'bold',
  },
  resultTextContainer: {
    flex: 1,
  },
  testName: {
    color: '#eee',
    fontSize: 14,
  },
  errorText: {
    color: '#ff4444',
    fontSize: 12,
    marginTop: 4,
    fontFamily: 'monospace',
  },
});
