import type { ReactNode } from 'react'

import './update-heading.css'

/** The bold line and the small line under it, as both windows draw them; `children` follow. */
export function UpdateHeading({ title, detail, children }: { title: string; detail?: string; children?: ReactNode }) {
  return (
    <div className="update-heading">
      <p className="update-heading__title">{title}</p>
      {detail && <p className="update-heading__detail">{detail}</p>}
      {children}
    </div>
  )
}
