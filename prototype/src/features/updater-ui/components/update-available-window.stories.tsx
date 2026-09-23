import type { Meta, StoryObj } from '@storybook/react-vite'
import { fn } from 'storybook/test'

import { appName, currentVersion, latestRelease, releaseWithNotesLink } from '../data'
import { UpdateAvailableWindow } from './update-available-window'

const meta = {
  id: 'features-updater-ui-update-available-window',
  title: 'Components/Update Available Window',
  component: UpdateAvailableWindow,
  args: {
    appName,
    currentVersion,
    release: latestRelease,
    automaticDownloads: { checked: false, onChange: fn() },
    onInstall: fn(),
    onLater: fn(),
    onSkip: fn(),
  },
} satisfies Meta<typeof UpdateAvailableWindow>

export default meta
type Story = StoryObj<typeof meta>

export const Available: Story = { name: 'Available' }

/** Already downloaded (automatic downloads, or Later on Ready): Install and Relaunch. */
export const Downloaded: Story = { name: 'Downloaded', args: { stage: 'downloaded' } }

/** Empty notes with a notes page: the page is linked and the window keeps its size. */
export const NotesLink: Story = { name: 'Available · Notes Link', args: { release: releaseWithNotesLink } }

/** Neither notes nor a notes page: the heading and the buttons alone. */
export const NoReleaseNotes: Story = {
  name: 'Available · No Release Notes',
  args: { release: { ...releaseWithNotesLink, releaseNotesUrl: undefined } },
}

/** `SetAutomaticDownloadsAllowed(false)`: no check box. */
export const DownloadsNotAllowed: Story = {
  name: 'Available · Automatic Downloads Not Allowed',
  args: { automaticDownloads: undefined },
}
