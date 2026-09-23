import type { DownloadUpdate } from './types'

/**
 * A download that reports progress in steps over `duration` milliseconds.
 * With `failure` set it rejects with that error once the bytes are in, the
 * way a package that fails signature verification does.
 */
export function simulateDownload({
  duration = 4000,
  failure,
}: { duration?: number; failure?: Error } = {}): DownloadUpdate {
  return (release, onProgress, signal) =>
    new Promise((resolve, reject) => {
      // A server that does not state the size still sends bytes.
      const total = release.size ?? 31_457_280
      const steps = 40
      let step = 0
      const timer = setInterval(() => {
        step += 1
        onProgress(Math.round((total * step) / steps))
        if (step < steps) return
        clearInterval(timer)
        if (failure) reject(failure)
        else resolve()
      }, duration / steps)
      signal.addEventListener('abort', () => {
        clearInterval(timer)
        reject(new DOMException('Download cancelled', 'AbortError'))
      })
    })
}
