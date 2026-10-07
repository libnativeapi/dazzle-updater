import { useState } from 'react'

import { AppIcon, Button, Checkbox, Window } from '../../../components/ui'
import { useStrings } from '../strings'
import { UpdateHeading } from './update-heading'
import { useWindowKeys } from './use-window-keys'

import './update-permission-window.css'

export interface UpdatePermissionWindowProps {
  appName: string
  /** Whether the app allows automatic downloads: without it, the check box is not shown. */
  automaticDownloadsAllowed?: boolean
  /** Check Automatically, with whether the user also wants updates downloaded. */
  onAllow: (automaticDownloads: boolean) => void
  /** Don't Check, Esc and the close button. */
  onDeny: () => void
}

/**
 * The one-time question before the first silent check — Sparkle's
 * `SUUpdatePermissionPrompt`, without its anonymous system profile, which
 * dazzle_updater does not collect.
 */
export function UpdatePermissionWindow({ appName, automaticDownloadsAllowed = true, onAllow, onDeny }: UpdatePermissionWindowProps) {
  const t = useStrings()
  const [downloads, setDownloads] = useState(false)
  useWindowKeys(() => onAllow(downloads), onDeny)

  return (
    <Window
      // SUUpdatePermissionPrompt.xib: Titled alone — no close button, no resize and no
      // title of its own — with a 437 pt content. Sparkle's anonymous system profile
      // section is not drawn: dazzle_updater does not collect one (specs/updater-ui.md).
      title=""
      controls={{ close: false, minimize: false, zoom: false }}
      width={437}
    >
      <div className="update-permission-window">
        <AppIcon size={64} />
        <div className="update-permission-window__body">
          <UpdateHeading title={t.permissionTitle} detail={t.permissionDetail(appName)} />
          {automaticDownloadsAllowed && (
            <Checkbox size="small" checked={downloads} onChange={setDownloads}>
              {t.permissionDownloads}
            </Checkbox>
          )}
          <div className="update-permission-window__buttons">
            <Button onClick={onDeny}>{t.dontCheck}</Button>
            <Button isDefault onClick={() => onAllow(downloads)}>
              {t.checkAutomatically}
            </Button>
          </div>
        </div>
      </div>
    </Window>
  )
}
