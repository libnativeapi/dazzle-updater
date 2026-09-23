/**
 * What the window reads from Updater about the update it found
 * (`GetUpdateDisplayVersion()`, `GetUpdateReleaseNotes()`, …).
 */
export interface Release {
  version: string
  displayVersion: string
  /** Markdown, as `GetUpdateReleaseNotes()` returns it; `''` when the release has none. */
  releaseNotes: string
  /** `GetUpdateReleaseNotesUrl()`: linked from the window when the notes are empty. */
  releaseNotesUrl?: string
  /** Size of the package in bytes; `null` when the server does not say. */
  size: number | null
}

/** Which call reported the error: it picks the Error state's headline. */
export type ErrorStep = 'check' | 'download' | 'install'

/** `Updater::GetErrorCode()` (specs/updater.md): picks the Error state's text. */
export type ErrorCode =
  | 'busy'
  | 'unsupported-platform'
  | 'network'
  | 'invalid-index'
  | 'signature'
  | 'cancelled'
  | 'permission'
  | 'app-location'
  | 'installer'

/** What a simulated Updater call rejects with: `GetErrorMessage()` and `GetErrorCode()`. */
export interface UpdaterError extends Error {
  code?: ErrorCode
}

export function updaterError(code: ErrorCode, message: string): UpdaterError {
  return Object.assign(new Error(message), { code })
}

/** What UpdaterUI remembers for the user (specs/updater-ui.md, "用户偏好"). */
export interface UpdaterUIPreferences {
  /** `null` until the user has answered the permission prompt. */
  automaticChecks: boolean | null
  automaticDownloads: boolean
  skippedVersion: string | null
}

/**
 * The window's states, as `specs/updater-ui.md` lists them, plus `hidden`: no
 * window on screen.
 */
export type UpdaterUIState =
  | { state: 'hidden' }
  | { state: 'checking' }
  | { state: 'available'; release: Release }
  | { state: 'downloaded'; release: Release }
  | { state: 'downloading'; release: Release; received: number }
  | { state: 'ready'; release: Release }
  | { state: 'installing'; release: Release }
  | { state: 'up-to-date' }
  | { state: 'error'; step: ErrorStep; code?: ErrorCode; message: string }

/** `Updater::CheckForUpdates()` and what it found; `null` means up to date. */
export type CheckForUpdates = () => Promise<Release | null>

/**
 * `Updater::DownloadUpdate()` with `GetDownloadProgress()` polled into
 * `onProgress`; an aborted signal is `CancelDownload()` and rejects with an
 * `AbortError`. Any other rejection carries `GetErrorMessage()`.
 */
export type DownloadUpdate = (
  release: Release,
  onProgress: (received: number) => void,
  signal: AbortSignal,
) => Promise<void>

/** `Updater::InstallUpdate()`: resolves when the installer has started and the app is to quit. */
export type InstallUpdate = () => Promise<void>
