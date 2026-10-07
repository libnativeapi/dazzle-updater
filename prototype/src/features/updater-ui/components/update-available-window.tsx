import { AppIcon, Button, Checkbox, Spinner, Window } from '../../../components/ui'
import { useStrings } from '../strings'
import type { Release } from '../types'
import { MarkdownView } from './markdown-view'
import { UpdateHeading } from './update-heading'
import { useWindowKeys } from './use-window-keys'

import './update-available-window.css'

/**
 * Sparkle's variants of this window that `specs/updater-ui.md` has not taken
 * up: shown only in the Sparkle Reference stories.
 */
export interface SparkleReferenceVariant {
  /** A critical update: no Skip and no Later. */
  critical?: boolean
  /** An update with no package: the default button opens its web page. */
  informationOnly?: boolean
  /** Release notes fetched from their own URL: still loading, or failed. */
  notes?: 'loading' | 'failed'
}

export interface UpdateAvailableWindowProps {
  appName: string
  currentVersion: string
  release: Release
  /** `downloaded`: the package is already here and verified, so the default is Install and Relaunch. */
  stage?: 'available' | 'downloaded'
  /** The automatic-downloads check box; left out when the app does not allow automatic downloads. */
  automaticDownloads?: { checked: boolean; onChange: (checked: boolean) => void }
  onInstall: () => void
  /** Later, Esc and the close button. */
  onLater: () => void
  onSkip: () => void
  sparkleReference?: SparkleReferenceVariant
}

/**
 * The window for the Available and Downloaded states — Sparkle's `SUUpdateAlert`:
 * what the new version is, its release notes, the automatic-downloads check box,
 * and Skip This Version ··· Remind Me Later · **Install**. Every measurement is
 * the xib's: see update-available-window.css for the constraint each one comes
 * from. It is 532 × 370pt with a 462 × 150 minimum and always resizable; when the
 * notes are empty the notes page is linked instead and the window keeps its size.
 */
export function UpdateAvailableWindow({
  appName,
  currentVersion,
  release,
  stage = 'available',
  automaticDownloads,
  onInstall,
  onLater,
  onSkip,
  sparkleReference = {},
}: UpdateAvailableWindowProps) {
  const t = useStrings()
  const showsNotes = hasReleaseNotes(release) || sparkleReference.notes !== undefined
  const downloaded = stage === 'downloaded'
  const { critical, informationOnly } = sparkleReference
  useWindowKeys(onInstall, critical ? undefined : onLater)

  const title = critical
    ? t.reference.criticalTitle(appName)
    : downloaded
      ? t.downloadedTitle(appName)
      : t.availableTitle(appName)
  const detail = informationOnly
    ? t.reference.informationDetail(appName, release.displayVersion, currentVersion)
    : critical
      ? t.reference.criticalDetail(appName, release.displayVersion, currentVersion)
      : downloaded
        ? t.downloadedDetail(appName, release.displayVersion)
        : t.availableDetail(appName, release.displayVersion, currentVersion)
  const install = informationOnly ? t.reference.learnMore : downloaded ? t.installAndRelaunch : t.installUpdate

  return (
    <Window
      // SUUpdateAlert.xib: "Software Update" in a transparent title bar, 532 × 370 with a
      // 462 × 150 minimum, resizable and miniaturisable.
      title={t.softwareUpdate}
      movableByBackground
      controls={{ close: !critical, minimize: true, zoom: true }}
      width={532}
      height={370}
      // The notes box keeps 200pt, so the window cannot shrink past its parts.
      resizable={{ minWidth: 462, minHeight: 150 }}
    >
      <div className="update-available-window">
        <div className="update-available-window__title-view">
          <AppIcon size={64} />
          <UpdateHeading title={title} detail={detail} />
        </div>
        {showsNotes ? (
          <div className="update-available-window__notes" aria-label="Release notes">
            {sparkleReference.notes === 'loading' ? (
              <div className="update-available-window__notes-status">
                <Spinner label="Loading release notes" />
              </div>
            ) : sparkleReference.notes === 'failed' ? (
              <p className="update-available-window__notes-failed">{t.reference.notesFailed}</p>
            ) : (
              <MarkdownView markdown={release.releaseNotes} />
            )}
          </div>
        ) : (
          release.releaseNotesUrl && (
            <a
              className="update-available-window__notes-link"
              href={release.releaseNotesUrl}
              title={release.releaseNotesUrl}
              // The system browser would open it; the prototype does not.
              onClick={event => event.preventDefault()}
            >
              {t.viewReleaseNotes}
            </a>
          )
        )}
        {automaticDownloads && !informationOnly && (
          <div className="update-available-window__options">
            <Checkbox size="small" checked={automaticDownloads.checked} onChange={automaticDownloads.onChange}>
              {t.automaticDownloads}
            </Checkbox>
          </div>
        )}
        <div className="update-available-window__buttons">
          {!critical && (
            <Button className="update-available-window__skip" onClick={onSkip}>
              {t.skip}
            </Button>
          )}
          <span className="update-available-window__spacer" />
          {!critical && (
            // Remind Me Later and Install Update are equal in width: SUUpdateAlert.xib
            // constrains their widths to each other (EWD-pD-s8E), which is what the pair
            // wrapper's equal columns reproduce.
            <span className="update-available-window__pair">
              <Button onClick={onLater}>{t.later}</Button>
              <Button isDefault onClick={onInstall}>
                {install}
              </Button>
            </span>
          )}
          {critical && (
            <Button isDefault onClick={onInstall}>
              {install}
            </Button>
          )}
        </div>
      </div>
    </Window>
  )
}

/** Whether the window shows a notes box — and so whether it can be resized. */
export function hasReleaseNotes(release: Release): boolean {
  return release.releaseNotes.trim() !== ''
}
