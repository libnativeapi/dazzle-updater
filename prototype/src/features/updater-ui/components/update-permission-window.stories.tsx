import type { Meta, StoryObj } from '@storybook/react-vite'
import { fn } from 'storybook/test'

import { appName } from '../data'
import { UpdatePermissionWindow } from './update-permission-window'

const meta = {
  id: 'features-updater-ui-update-permission-window',
  title: 'Components/Update Permission Window',
  component: UpdatePermissionWindow,
  args: { appName, onAllow: fn(), onDeny: fn() },
} satisfies Meta<typeof UpdatePermissionWindow>

export default meta
type Story = StoryObj<typeof meta>

export const FirstLaunch: Story = { name: 'First Launch' }

/** `SetAutomaticDownloadsAllowed(false)`: the question alone. */
export const DownloadsNotAllowed: Story = {
  name: 'Automatic Downloads Not Allowed',
  args: { automaticDownloadsAllowed: false },
}
