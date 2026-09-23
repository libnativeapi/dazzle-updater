import type { Meta, StoryObj } from '@storybook/react-vite'

import { latestRelease } from '../data'
import { MarkdownView } from './markdown-view'

const meta = {
  id: 'features-updater-ui-markdown-view',
  title: 'Components/Markdown View',
  component: MarkdownView,
  decorators: [
    Story => (
      <div style={{ width: 480, background: 'var(--text-bg)', border: '1px solid var(--separator)', borderRadius: 6 }}>
        <Story />
      </div>
    ),
  ],
} satisfies Meta<typeof MarkdownView>

export default meta
type Story = StoryObj<typeof meta>

/** A release's notes, as the update window shows them. */
export const ReleaseNotes: Story = { name: 'Release Notes', args: { markdown: latestRelease.releaseNotes } }

/** Every construct the subset supports, and an image, which is dropped. */
export const SupportedSubset: Story = {
  name: 'Supported Subset',
  args: {
    markdown: `# Heading 1
## Heading 2
### Heading 3

A paragraph with **bold**, *italic*, \`inline code\` and a [link](https://example.com).
Lines of one paragraph join into one.

- Unordered item
- Another item

1. Ordered item
2. Another item

\`\`\`
code block
  keeps its indentation
\`\`\`

![a remote image is not loaded](https://example.com/screenshot.png)`,
  },
}
