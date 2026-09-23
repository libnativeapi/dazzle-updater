import { useEffect, useRef, useState } from 'react'

import { AppIcon, Button, Window } from '../../components/ui'

import { Desktop, type DesktopWindow } from './components/desktop'
import { UpdateAvailableWindow } from './components/update-available-window'
import { UpdatePermissionWindow } from './components/update-permission-window'
import { UpdateStatusWindow } from './components/update-status-window'
import type {
  CheckForUpdates,
  DownloadUpdate,
  ErrorStep,
  InstallUpdate,
  Release,
  UpdaterError,
  UpdaterUIPreferences,
  UpdaterUIState,
} from './types'

import './updater-ui-view.css'

export interface UpdaterUIViewProps {
  appName: string
  currentVersion: string
  check: CheckForUpdates
  download: DownloadUpdate
  install: InstallUpdate
  /**
   * How the view starts: `user` calls `CheckForUpdates()` on mount, `background`
   * calls `CheckForUpdatesInBackground()`, and a state draws that state as is.
   */
  start?: 'user' | 'background' | UpdaterUIState
  /** What the user has told UpdaterUI before this run. */
  preferences?: UpdaterUIPreferences
  /** `SetAutomaticDownloadsAllowed()`: false hides every automatic-downloads check box. */
  automaticDownloadsAllowed?: boolean
}

/** What is on screen: one of the round's states, or the one-time permission prompt. */
type Shown = UpdaterUIState | { state: 'permission' }

const DEFAULT_PREFERENCES: UpdaterUIPreferences = { automaticChecks: true, automaticDownloads: false, skippedVersion: null }

/**
 * `UpdaterUI` as `specs/updater-ui.md` describes it: the state machine, the
 * user's preferences, and the windows that draw it — `UpdateAvailableWindow`
 * for Available and Downloaded, `UpdateStatusWindow` for every other state,
 * `UpdatePermissionWindow` before the first silent check. Behind them stands
 * the host app: its two entry points, what the last round left behind, and
 * what UpdaterUI currently remembers for the user.
 */
export function UpdaterUIView({
  appName,
  currentVersion,
  check,
  download,
  install,
  start = 'user',
  preferences: initialPreferences = DEFAULT_PREFERENCES,
  automaticDownloadsAllowed = true,
}: UpdaterUIViewProps) {
  const [ui, setUi] = useState<Shown>(typeof start === 'string' ? { state: 'hidden' } : start)
  const [preferences, setPreferences] = useState(initialPreferences)
  // Kept for the rest of the process after Later on Ready or Downloaded; a new
  // check would make Updater clear it, so UpdaterUI shows it instead of checking.
  const [downloaded, setDownloaded] = useState<Release | null>(null)
  // What the host says under its buttons while no window is up.
  const [note, setNote] = useState<string | null>(null)
  // Updater's check cannot be interrupted: Cancel only closes the window, and a
  // second request while it runs shows the window again on the same check.
  const pendingCheck = useRef<{ promise: Promise<Release | null>; silent: boolean } | null>(null)
  const abort = useRef<AbortController | null>(null)
  // A state change from a round the window has since left must not land.
  const round = useRef(0)

  function remember(change: Partial<UpdaterUIPreferences>) {
    setPreferences(value => ({ ...value, ...change }))
  }

  function hide() {
    round.current += 1
    abort.current?.abort()
    abort.current = null
    setUi({ state: 'hidden' })
  }

  function fail(step: ErrorStep, error: UpdaterError) {
    setUi({ state: 'error', step, code: error.code, message: error.message })
  }

  function runCheck(silent: boolean): Promise<Release | null> {
    let pending = pendingCheck.current
    if (pending) {
      // The latest request decides whether the running check's answer is shown.
      pending.silent = silent
    } else {
      pending = { promise: check(), silent }
      pendingCheck.current = pending
      const clear = () => {
        if (pendingCheck.current === pending) pendingCheck.current = null
      }
      pending.promise.then(clear, clear)
    }
    return pending.promise
  }

  /** `CheckForUpdates()`: the user asked, so every answer is shown. */
  function checkForUpdates() {
    setNote(null)
    if (downloaded) {
      round.current += 1
      setUi({ state: 'downloaded', release: downloaded })
      return
    }
    const current = ++round.current
    setUi({ state: 'checking' })
    runCheck(false).then(
      release => {
        if (round.current !== current) return
        // A skipped version is still shown: the user asked for it.
        setUi(release ? { state: 'available', release } : { state: 'up-to-date' })
      },
      (error: UpdaterError) => {
        if (round.current === current) fail('check', error)
      },
    )
  }

  /** `CheckForUpdatesInBackground()`: only an update the user has not skipped is shown. */
  function checkForUpdatesInBackground(answered = preferences) {
    if (downloaded) return
    if (answered.automaticChecks === false) {
      setNote('Automatic checks are off: the silent check did nothing.')
      return
    }
    if (answered.automaticChecks === null) {
      round.current += 1
      setUi({ state: 'permission' })
      return
    }
    const current = ++round.current
    setNote('Checking in the background — a window appears only if an update is found.')
    runCheck(true).then(
      release => {
        if (round.current !== current) return
        if (!release) return setNote('The silent check found nothing new, so nothing was shown.')
        if (release.version === answered.skippedVersion) {
          return setNote(`The silent check found ${release.displayVersion}, which the user skipped, so nothing was shown.`)
        }
        // A newer version than the skipped one supersedes the skip.
        if (answered.skippedVersion) remember({ skippedVersion: null })
        if (automaticDownloadsAllowed && answered.automaticDownloads) return downloadSilently(release, current)
        setNote(null)
        setUi({ state: 'available', release })
      },
      () => {
        if (round.current === current) setNote('The silent check failed, so nothing was shown.')
      },
    )
  }

  /** Automatic downloads: fetched with no window, then shown as Downloaded. */
  function downloadSilently(release: Release, current: number) {
    const controller = new AbortController()
    abort.current = controller
    setNote(`Found ${release.displayVersion}; downloading it in the background…`)
    download(release, () => {}, controller.signal).then(
      () => {
        if (round.current !== current) return
        setNote(null)
        setUi({ state: 'downloaded', release })
      },
      () => {
        if (round.current === current) setNote('The background download failed, so nothing was shown.')
      },
    )
  }

  function runDownload(release: Release) {
    const current = ++round.current
    const controller = new AbortController()
    abort.current = controller
    setUi({ state: 'downloading', release, received: 0 })
    download(release, received => setUi({ state: 'downloading', release, received }), controller.signal).then(
      () => {
        if (round.current === current) setUi({ state: 'ready', release })
      },
      (error: UpdaterError) => {
        if (error.name !== 'AbortError' && round.current === current) fail('download', error)
      },
    )
  }

  function runInstall(release: Release) {
    const current = ++round.current
    setUi({ state: 'installing', release })
    install().then(
      () => {
        if (round.current !== current) return
        // UpdaterUI asks the app to quit; the prototype's app "quits" here.
        setDownloaded(null)
        setUi({ state: 'hidden' })
        setNote(`${appName} quit so the installer could replace it.`)
      },
      (error: UpdaterError) => {
        if (round.current === current) fail('install', error)
      },
    )
  }

  /** Later on Ready or Downloaded: the package stays for the rest of the process. */
  function keepForLater(release: Release) {
    setDownloaded(release)
    hide()
    setNote(`${release.displayVersion} stays downloaded: Check for Updates… shows it again without checking.`)
  }

  function skip(release: Release) {
    remember({ skippedVersion: release.version })
    setDownloaded(null)
    hide()
    setNote(`Skipped ${release.displayVersion}: silent checks will not show it again.`)
  }

  useEffect(() => {
    if (start === 'user') checkForUpdates()
    else if (start === 'background') checkForUpdatesInBackground()
    return () => {
      round.current += 1
      abort.current?.abort()
    }
    // Runs once per mount: a story remounts the view to start over.
  }, [])

  let shown: DesktopWindow | null = null
  if (ui.state === 'permission') {
    shown = {
      key: 'permission',
      node: (
        <UpdatePermissionWindow
          appName={appName}
          automaticDownloadsAllowed={automaticDownloadsAllowed}
          onAllow={withDownloads => {
            const answered = { ...preferences, automaticChecks: true, automaticDownloads: withDownloads }
            setPreferences(answered)
            setUi({ state: 'hidden' })
            // Then the silent check it was asked before carries on.
            checkForUpdatesInBackground(answered)
          }}
          onDeny={() => {
            remember({ automaticChecks: false })
            hide()
            setNote('Automatic checks are off: this and later silent checks do nothing.')
          }}
        />
      ),
    }
  } else if (ui.state === 'available' || ui.state === 'downloaded') {
    const release = ui.release
    const isDownloaded = ui.state === 'downloaded'
    shown = {
      key: 'available',
      node: (
        <UpdateAvailableWindow
          appName={appName}
          currentVersion={currentVersion}
          release={release}
          stage={ui.state}
          automaticDownloads={
            automaticDownloadsAllowed
              ? { checked: preferences.automaticDownloads, onChange: checked => remember({ automaticDownloads: checked }) }
              : undefined
          }
          onInstall={() => (isDownloaded ? runInstall(release) : runDownload(release))}
          onLater={() => (isDownloaded ? keepForLater(release) : hide())}
          onSkip={() => skip(release)}
        />
      ),
    }
  } else if (ui.state !== 'hidden') {
    const status = ui
    shown = {
      key: 'status',
      node: (
        <UpdateStatusWindow
          appName={appName}
          currentVersion={currentVersion}
          status={status}
          onInstallAndRelaunch={() => status.state === 'ready' && runInstall(status.release)}
          onDismiss={() => (status.state === 'ready' ? keepForLater(status.release) : hide())}
        />
      ),
    }
  }

  // The app that embeds dazzle; Desktop decides which window is key.
  const host = (
    <Window title={appName} width={460}>
      <div className="updater-ui-view__host">
        <AppIcon size={64} />
        <p className="updater-ui-view__host-title">
          {appName} {currentVersion}
        </p>
        {note && <p className="updater-ui-view__host-note">{note}</p>}
        <div className="updater-ui-view__host-actions">
          <Button onClick={() => checkForUpdatesInBackground()}>Check in Background</Button>
          <Button isDefault onClick={checkForUpdates}>
            Check for Updates…
          </Button>
        </div>
        <dl className="updater-ui-view__preferences" aria-label="UpdaterUI preferences">
          <dt>Automatic checks</dt>
          <dd>{preferences.automaticChecks === null ? 'not asked yet' : preferences.automaticChecks ? 'on' : 'off'}</dd>
          <dt>Automatic downloads</dt>
          <dd>{automaticDownloadsAllowed ? (preferences.automaticDownloads ? 'on' : 'off') : 'not allowed by the app'}</dd>
          <dt>Skipped version</dt>
          <dd>{preferences.skippedVersion ?? 'none'}</dd>
          <dt>Downloaded, kept for later</dt>
          <dd>{downloaded ? downloaded.displayVersion : 'none'}</dd>
        </dl>
        <Button
          onClick={() => {
            setPreferences({ automaticChecks: null, automaticDownloads: false, skippedVersion: null })
            setDownloaded(null)
            setNote('Preferences reset: the next silent check asks first.')
          }}
        >
          Reset Preferences
        </Button>
      </div>
    </Window>
  )

  return <Desktop window={shown}>{host}</Desktop>
}
