import type { CheckForUpdates, Release } from './types'

export type CheckOutcome = 'available' | 'up-to-date' | 'failed'

/**
 * A check that answers after a short delay, as a request to the release index
 * would. `outcome` decides the answer, so every branch of the window is
 * reachable from a story.
 */
export function simulateCheck(
  release: Release,
  outcome: CheckOutcome,
  failure: Error,
  delay = 1200,
): CheckForUpdates {
  return () =>
    new Promise((resolve, reject) => {
      setTimeout(() => {
        if (outcome === 'failed') reject(failure)
        else resolve(outcome === 'available' ? release : null)
      }, delay)
    })
}
