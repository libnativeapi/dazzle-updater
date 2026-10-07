import type { ReactNode } from 'react'

import { AppIcon, Button, ProgressBar, Window } from '../../../components/ui'
import { formatBytes } from '../format'
import { useStrings } from '../strings'
import type { Release, UpdaterUIState } from '../types'
import { UpdateHeading } from './update-heading'
import { useWindowKeys } from './use-window-keys'

import './update-status-window.css'

/**
 * Sparkle's states that `specs/updater-ui.md` has not taken up: shown only in
 * the Sparkle Reference stories.
 */
export type SparkleReferenceStatus =
  /** Up to date, with Sparkle's Version History button. */
  | { state: 'up-to-date-history' }
  /** An update exists but this machine cannot install it. */
  | { state: 'unsupported'; reason: 'system-too-old' | 'system-too-new' | 'apple-silicon'; release: Release; required?: string }
  /** Shown after the relaunch. */
  | { state: 'installed'; release: Release }

/** Every state the status window draws: all of the window's states but Available and Downloaded. */
export type UpdateStatus = Exclude<UpdaterUIState, { state: 'hidden' | 'available' | 'downloaded' }> | SparkleReferenceStatus

export interface UpdateStatusWindowProps {
  appName: string
  currentVersion: string
  status: UpdateStatus
  onInstallAndRelaunch: () => void
  /** Cancel, Later, OK, Esc and the close button. */
  onDismiss: () => void
}

/**
 * The window for every other state — Sparkle's `SUStatusController`: the icon, a
 * message or a progress bar, and the state's buttons, in one 400 × 107 layout (see
 * update-status-window.css for the constraint each measurement comes from).
 *
 * Sparkle reports "up to date" and update errors in an NSAlert rather than in this
 * window; specs/updater-ui.md keeps them here so that the flow has one place to
 * look, which is the one place this window knowingly departs from Sparkle.
 */
export function UpdateStatusWindow({ appName, currentVersion, status, onInstallAndRelaunch, onDismiss }: UpdateStatusWindowProps) {
  const t = useStrings()

  let body: ReactNode
  let statusText: string | undefined
  let buttons: ReactNode
  switch (status.state) {
    case 'checking':
      body = <Progress title={t.checking} />
      buttons = <Button onClick={onDismiss}>{t.cancel}</Button>
      break
    case 'downloading': {
      const total = status.release.size
      body = <Progress title={t.downloading} value={total ? status.received / total : undefined} />
      statusText = total
        ? t.downloadedOf(formatBytes(status.received), formatBytes(total))
        : t.downloaded(formatBytes(status.received))
      buttons = <Button onClick={onDismiss}>{t.cancel}</Button>
      break
    }
    case 'ready':
      body = <Progress title={t.readyTitle} value={1} />
      buttons = (
        <>
          <Button onClick={onDismiss}>{t.later}</Button>
          <Button isDefault onClick={onInstallAndRelaunch}>
            {t.installAndRelaunch}
          </Button>
        </>
      )
      break
    case 'installing':
      body = <Progress title={t.installing} />
      break
    case 'up-to-date':
      body = <UpdateHeading title={t.upToDateTitle} detail={t.upToDateDetail(appName, currentVersion)} />
      buttons = (
        <Button isDefault onClick={onDismiss}>
          {t.ok}
        </Button>
      )
      break
    case 'error': {
      // The text by error code, and GetErrorMessage() under it for a bug report.
      const texts = t.errorText(appName)
      body = (
        // Sparkle titles every update failure "Update Error!" and explains it below.
        <UpdateHeading title={t.errorTitle} detail={(status.code && texts[status.code]) || texts.other}>
          <p className="update-status-window__message">{status.message}</p>
        </UpdateHeading>
      )
      buttons = (
        <Button isDefault onClick={onDismiss}>
          {t.ok}
        </Button>
      )
      break
    }
    case 'up-to-date-history':
      body = <UpdateHeading title={t.upToDateTitle} detail={t.upToDateDetail(appName, currentVersion)} />
      buttons = (
        <>
          <Button onClick={onDismiss}>{t.reference.versionHistory}</Button>
          <Button isDefault onClick={onDismiss}>
            {t.ok}
          </Button>
        </>
      )
      break
    case 'unsupported': {
      const version = status.release.displayVersion
      const required = status.required ?? ''
      const [title, detail] =
        status.reason === 'system-too-old'
          ? [t.reference.tooOldTitle, t.reference.tooOldDetail(appName, version, required)]
          : status.reason === 'system-too-new'
            ? [t.reference.tooNewTitle, t.reference.tooNewDetail(appName, version, required)]
            : [t.reference.appleSiliconTitle, t.reference.appleSiliconDetail(appName, version)]
      body = <UpdateHeading title={title} detail={detail} />
      buttons = (
        <>
          <Button onClick={onDismiss}>{t.reference.learnMore}</Button>
          <Button isDefault onClick={onDismiss}>
            {t.ok}
          </Button>
        </>
      )
      break
    }
    case 'installed':
      body = <UpdateHeading title={t.reference.installedTitle} detail={t.reference.installedDetail(appName, status.release.displayVersion)} />
      buttons = (
        <Button isDefault onClick={onDismiss}>
          {t.ok}
        </Button>
      )
      break
  }

  const showsProgress = ['checking', 'downloading', 'ready', 'installing'].includes(status.state)
  const found = status.state === 'downloading' || status.state === 'ready' || status.state === 'installing'
  const dismissible = status.state !== 'installing'
  const defaultAction =
    status.state === 'ready' ? onInstallAndRelaunch : !found && status.state !== 'checking' ? onDismiss : undefined
  useWindowKeys(defaultAction, dismissible ? onDismiss : undefined)

  return (
    <Window
      // SUStatusController.m: 400 × 107 with a 213 × 107 minimum. "Software Update"
      // while checking; "Updating <app>" once an update is on its way.
      title={found ? t.updating(appName) : t.softwareUpdate}
      controls={{ close: dismissible, minimize: false, zoom: false }}
      width={400}
    >
      {/* A progress bar sits in a frame with room under it; a message does not. */}
      <div className="update-status-window" data-body={showsProgress ? 'progress' : 'message'}>
        <div className="update-status-window__icon">
          <AppIcon size={64} />
        </div>
        <div className="update-status-window__body">{body}</div>
        <div className="update-status-window__footer">
          <span className="update-status-window__status">{statusText}</span>
          {buttons}
        </div>
      </div>
    </Window>
  )
}

function Progress({ title, value }: { title: string; value?: number }) {
  return (
    <div className="update-status-window__progress">
      <p className="update-heading__title">{title}</p>
      {/* The bar sits in a 20pt frame, as NSProgressIndicator's does. */}
      <div className="update-status-window__bar">
        <ProgressBar value={value} label={title} />
      </div>
    </div>
  )
}
