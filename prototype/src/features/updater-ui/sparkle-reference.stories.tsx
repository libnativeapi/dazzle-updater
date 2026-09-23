import type { Meta, StoryObj } from '@storybook/react-vite'
import { fn } from 'storybook/test'

import { Desktop } from './components/desktop'
import { UpdateAvailableWindow } from './components/update-available-window'
import { UpdateStatusWindow } from './components/update-status-window'
import { appName, currentVersion, latestRelease } from './data'

/**
 * What Sparkle's standard user interface does that `specs/updater-ui.md` has
 * not taken up. Kept apart from Components and Examples so none of it is taken
 * for UpdaterUI's behaviour: each story names the Sparkle source it follows.
 */
const meta = {
  id: 'features-updater-ui-sparkle-reference',
  title: 'Reference/Sparkle',
  parameters: { layout: 'centered' },
} satisfies Meta

export default meta
type Story = StoryObj<typeof meta>

const available = {
  appName,
  currentVersion,
  release: latestRelease,
  onInstall: fn(),
  onLater: fn(),
  onSkip: fn(),
}

const status = { appName, currentVersion, onInstallAndRelaunch: fn(), onDismiss: fn() }

/** `SUAppcastItem.criticalUpdate` (SUUpdateAlert.m): no Skip, no Later, and the window cannot be closed. */
export const CriticalUpdate: Story = {
  name: 'Critical Update',
  render: () => <UpdateAvailableWindow {...available} sparkleReference={{ critical: true }} />,
}

/** `informationOnlyUpdate` (SUUpdateAlert.m): no package, so the default button opens the release's page. */
export const InformationOnly: Story = {
  name: 'Information-Only Update',
  render: () => <UpdateAvailableWindow {...available} sparkleReference={{ informationOnly: true }} />,
}

/** `displayReleaseNotesSpinner` (SUUpdateAlert.m): notes fetched from their own URL. */
export const NotesLoading: Story = {
  name: 'Release Notes · Loading',
  render: () => <UpdateAvailableWindow {...available} sparkleReference={{ notes: 'loading' }} />,
}

/** The release notes could not be downloaded (SUUpdateAlert.m). */
export const NotesFailed: Story = {
  name: 'Release Notes · Failed',
  render: () => <UpdateAvailableWindow {...available} sparkleReference={{ notes: 'failed' }} />,
}

/** `showUpdateNotFoundWithError:` (SPUStandardUserDriver.m): Version History beside OK. */
export const VersionHistory: Story = {
  name: 'Up to Date · Version History',
  render: () => <UpdateStatusWindow {...status} status={{ state: 'up-to-date-history' }} />,
}

/** `SPUNoUpdateFoundReasonSystemIsTooOld` (SPUNoUpdateFoundInfo.m). */
export const SystemTooOld: Story = {
  name: 'Unsupported · macOS Too Old',
  render: () => (
    <UpdateStatusWindow {...status} status={{ state: 'unsupported', reason: 'system-too-old', release: latestRelease, required: '14.0' }} />
  ),
}

/** `SPUNoUpdateFoundReasonSystemIsTooNew` (SPUNoUpdateFoundInfo.m). */
export const SystemTooNew: Story = {
  name: 'Unsupported · macOS Too New',
  render: () => (
    <UpdateStatusWindow {...status} status={{ state: 'unsupported', reason: 'system-too-new', release: latestRelease, required: '15.7' }} />
  ),
}

/** `SPUNoUpdateFoundReasonHardwareDoesNotSupportARM64` (SPUNoUpdateFoundInfo.m). */
export const AppleSilicon: Story = {
  name: 'Unsupported · Apple Silicon Required',
  render: () => <UpdateStatusWindow {...status} status={{ state: 'unsupported', reason: 'apple-silicon', release: latestRelease }} />,
}

/** `showUpdateInstalledAndRelaunched:` (SPUStandardUserDriver.m): after the relaunch. */
export const UpdateInstalled: Story = {
  name: 'Update Installed',
  render: () => <UpdateStatusWindow {...status} status={{ state: 'installed', release: latestRelease }} />,
}

/** `requestUserAttention:` (SPUStandardUserDriver.m): the Dock icon bounces when the update is ready. macOS only. */
export const DockAttention: Story = {
  name: 'Ready · Dock Attention',
  parameters: { layout: 'fullscreen' },
  render: () => (
    <Desktop
      dock={{ attention: true }}
      window={{ key: 'status', node: <UpdateStatusWindow {...status} status={{ state: 'ready', release: latestRelease }} /> }}
    >
      {null}
    </Desktop>
  ),
}
