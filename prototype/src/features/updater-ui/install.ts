import type { InstallUpdate } from './types'

/**
 * An installer that starts after `duration` milliseconds. With `failure` set it
 * rejects with that error, as `InstallUpdate()` returning false would.
 */
export function simulateInstall({ duration = 1200, failure }: { duration?: number; failure?: Error } = {}): InstallUpdate {
  return () =>
    new Promise((resolve, reject) => {
      setTimeout(() => (failure ? reject(failure) : resolve()), duration)
    })
}
