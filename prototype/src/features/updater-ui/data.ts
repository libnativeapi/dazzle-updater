import { updaterError, type Release, type UpdaterUIPreferences } from './types'

export const appName = 'DazzleUpdater Demo'

export const currentVersion = '1.3.2'

export const latestRelease: Release = {
  version: '1.4.0',
  displayVersion: '1.4',
  size: 46_000_000,
  releaseNotesUrl: 'https://example.com/dazzle-demo/changelog#1.4.0',
  releaseNotes: `## New

- **Dark mode** follows the system appearance.
- Export a project as a single archive from *File ▸ Export*.
- Recent projects can be pinned to the top of the list.

## Fixed

1. A crash on launch when the recent-projects list was empty.
2. The window position is restored on the display it was closed on.
3. \`dazzle-demo --open\` accepts paths with spaces.

To turn the new behaviour off:

\`\`\`
defaults write com.example.dazzle-demo FollowSystemAppearance -bool NO
\`\`\`

See the [full changelog](https://example.com/dazzle-demo/changelog) for everything else.
`,
}

/** A release with no notes but a notes page: the window links to it instead. */
export const releaseWithNotesLink: Release = { ...latestRelease, releaseNotes: '' }

/** A server that does not state the package size: the bar is indeterminate. */
export const releaseWithoutSize: Release = { ...latestRelease, size: null }

/** A user who has not been asked yet: the first silent check shows the permission prompt. */
export const freshPreferences: UpdaterUIPreferences = {
  automaticChecks: null,
  automaticDownloads: false,
  skippedVersion: null,
}

/** A user who agreed to automatic checks, and nothing else. */
export const answeredPreferences: UpdaterUIPreferences = { ...freshPreferences, automaticChecks: true }

/** A check that cannot reach the release index. */
export const checkFailure = updaterError('network', 'could not reach https://example.com/dazzle-demo/releases.json (timed out)')

/** A package that fails signature verification. */
export const signatureFailure = updaterError('signature', 'the update package signature is invalid')

/** An installer that could not be started. */
export const installFailure = updaterError('installer', 'the installer could not be launched')

/** An app running from a read-only disk image. */
export const appLocationFailure = updaterError('app-location', 'the application is on a read-only volume: /Volumes/DazzleUpdater Demo')

/** An app the user may not replace. */
export const permissionFailure = updaterError('permission', 'permission denied: /Applications/DazzleUpdater Demo.app')
