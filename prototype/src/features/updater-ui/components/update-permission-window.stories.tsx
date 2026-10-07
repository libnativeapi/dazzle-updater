import type { Meta, StoryObj } from '@storybook/react-vite'
import { fn } from 'storybook/test'

import { appName } from '../data'
import { UpdatePermissionWindow } from './update-permission-window'

const meta = {
  id: 'features-updater-ui-update-permission-window',
  title: 'Components/Update Permission Window',
  component: UpdatePermissionWindow,
  parameters: {
    docs: {
      description: {
        component:
          'Sparkle 2.x `Sparkle/SUUpdatePermissionPrompt.xib`, one to one in the parts dazzle_updater keeps: content 437 pt wide, titled with no close button and no title of its own, the icon 64 pt at (23, 18), the text column at 106 pt with the question 20 pt from the top, and the two answers 14 pt apart, 20 pt from the bottom. Sparkle’s anonymous system profile section is not drawn.',
      },
    },
  },
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
