import { TurboModuleRegistry, type TurboModule } from 'react-native';

export interface Spec extends TurboModule {
  install(bundleId: string): boolean;
}

export default TurboModuleRegistry.getEnforcing<Spec>('Network');
