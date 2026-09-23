import type { Meta, StoryObj } from '@storybook/react-vite'

import { simulateCheck } from './check'
import {
  answeredPreferences,
  appLocationFailure,
  appName,
  checkFailure,
  currentVersion,
  freshPreferences,
  installFailure,
  latestRelease,
  permissionFailure,
  signatureFailure,
} from './data'
import { simulateDownload } from './download'
import { simulateInstall } from './install'
import { UpdaterUIView } from './updater-ui-view'

const meta = {
  id: 'features-updater-ui-overview',
  title: 'Examples/Host App',
  component: UpdaterUIView,
  // The view is the whole screen: windows are placed on it and can be dragged.
  parameters: { layout: 'fullscreen' },
  args: {
    appName,
    currentVersion,
    check: simulateCheck(latestRelease, 'available', checkFailure),
    download: simulateDownload(),
    install: simulateInstall(),
    preferences: answeredPreferences,
  },
  // Remount per story, so each scenario starts its round over.
  render: args => <UpdaterUIView key={JSON.stringify([args.start, args.preferences])} {...args} />,
} satisfies Meta<typeof UpdaterUIView>

export default meta
type Story = StoryObj<typeof meta>

/** `CheckForUpdates()` from the menu item: Checking, Available, Downloading, Ready, Installing. */
export const CheckForUpdates: Story = { name: 'Check for Updates' }

/** `CheckForUpdatesInBackground()`: the window appears only once an update is found. */
export const CheckInBackground: Story = { name: 'Check in Background', args: { start: 'background' } }

/** Never asked: the silent check asks first, then carries on with the answer. */
export const FirstLaunch: Story = {
  name: 'Check in Background · First Launch',
  args: { start: 'background', preferences: freshPreferences },
}

/** Automatic downloads on: the update is fetched with no window, then shown ready to install. */
export const AutomaticDownload: Story = {
  name: 'Check in Background · Automatic Download',
  args: { start: 'background', preferences: { ...answeredPreferences, automaticDownloads: true } },
}

/** The update found is the version the user skipped: nothing is shown. */
export const SkippedInBackground: Story = {
  name: 'Check in Background · Skipped Version',
  args: { start: 'background', preferences: { ...answeredPreferences, skippedVersion: latestRelease.version } },
}

/** The user asked, so a skipped version is shown anyway. */
export const SkippedOnRequest: Story = {
  name: 'Check for Updates · Skipped Version',
  args: { preferences: { ...answeredPreferences, skippedVersion: latestRelease.version } },
}

/** The user said Don’t Check: silent checks do nothing. */
export const AutomaticChecksOff: Story = {
  name: 'Automatic Checks Off',
  args: { start: 'background', preferences: { ...answeredPreferences, automaticChecks: false } },
}

/** `SetAutomaticDownloadsAllowed(false)`: no check box anywhere. */
export const DownloadsNotAllowed: Story = {
  name: 'Automatic Downloads Not Allowed',
  args: { automaticDownloadsAllowed: false },
}

export const UpToDate: Story = {
  name: 'Up to Date',
  args: { check: simulateCheck(latestRelease, 'up-to-date', checkFailure) },
}

export const CheckError: Story = {
  name: 'Error · Network',
  args: { check: simulateCheck(latestRelease, 'failed', checkFailure) },
}

/** The package downloads, then fails signature verification. */
export const DownloadError: Story = {
  name: 'Error · Signature',
  args: {
    start: { state: 'available', release: latestRelease },
    download: simulateDownload({ duration: 2000, failure: signatureFailure }),
  },
}

export const InstallError: Story = {
  name: 'Error · Installer',
  args: { start: { state: 'ready', release: latestRelease }, install: simulateInstall({ failure: installFailure }) },
}

export const AppLocationError: Story = {
  name: 'Error · App Location',
  args: { start: { state: 'ready', release: latestRelease }, install: simulateInstall({ failure: appLocationFailure }) },
}

export const PermissionError: Story = {
  name: 'Error · Permission',
  args: { start: { state: 'ready', release: latestRelease }, install: simulateInstall({ failure: permissionFailure }) },
}
