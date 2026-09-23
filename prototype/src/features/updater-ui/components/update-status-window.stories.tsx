import type { Meta, StoryObj } from '@storybook/react-vite'
import { fn } from 'storybook/test'

import {
  appLocationFailure,
  appName,
  checkFailure,
  currentVersion,
  installFailure,
  latestRelease,
  permissionFailure,
  releaseWithoutSize,
  signatureFailure,
} from '../data'
import type { ErrorStep, UpdaterError } from '../types'
import { UpdateStatusWindow } from './update-status-window'

const meta = {
  id: 'features-updater-ui-update-status-window',
  title: 'Components/Update Status Window',
  component: UpdateStatusWindow,
  args: {
    appName,
    currentVersion,
    status: { state: 'checking' },
    onInstallAndRelaunch: fn(),
    onDismiss: fn(),
  },
} satisfies Meta<typeof UpdateStatusWindow>

export default meta
type Story = StoryObj<typeof meta>

export const Checking: Story = { name: 'Checking' }

export const Downloading: Story = {
  name: 'Downloading',
  args: { status: { state: 'downloading', release: latestRelease, received: 12_300_000 } },
}

export const DownloadingUnknownSize: Story = {
  name: 'Downloading · Unknown Size',
  args: { status: { state: 'downloading', release: releaseWithoutSize, received: 12_300_000 } },
}

export const Ready: Story = { name: 'Ready', args: { status: { state: 'ready', release: latestRelease } } }

export const Installing: Story = { name: 'Installing', args: { status: { state: 'installing', release: latestRelease } } }

export const UpToDate: Story = { name: 'Up to Date', args: { status: { state: 'up-to-date' } } }

/** An Error status as UpdaterUI builds it from Updater's code and message. */
function error(step: ErrorStep, failure: UpdaterError) {
  return { status: { state: 'error' as const, step, code: failure.code, message: failure.message } }
}

export const NetworkError: Story = { name: 'Error · Network', args: error('check', checkFailure) }

export const InvalidIndexError: Story = {
  name: 'Error · Invalid Index',
  args: error('check', { name: 'Error', code: 'invalid-index', message: 'unsupported release index schema' }),
}

export const SignatureError: Story = { name: 'Error · Signature', args: error('download', signatureFailure) }

export const InstallerError: Story = { name: 'Error · Installer', args: error('install', installFailure) }

export const AppLocationError: Story = { name: 'Error · App Location', args: error('install', appLocationFailure) }

export const PermissionError: Story = { name: 'Error · Permission', args: error('install', permissionFailure) }

/** A code UpdaterUI has no text for: the generic text, and the message under it. */
export const UnknownError: Story = {
  name: 'Error · Unknown',
  args: error('check', { name: 'Error', message: 'unexpected response: 418' }),
}
