import type { ReactNode } from 'react'

import './checkbox.css'

export interface CheckboxProps {
  checked: boolean
  onChange: (checked: boolean) => void
  /** `small` is `NSControlSizeSmall`: a smaller box beside a small label. */
  size?: 'regular' | 'small'
  children: ReactNode
}

/** A check box with its label, in the desktop's own style. */
export function Checkbox({ checked, onChange, size = 'regular', children }: CheckboxProps) {
  return (
    <label className="checkbox" data-size={size}>
      <input type="checkbox" checked={checked} onChange={event => onChange(event.target.checked)} />
      <span className="checkbox__box" aria-hidden>
        <svg viewBox="0 0 12 12"><path d="M2.6 6.3l2.3 2.3 4.5-5.1" /></svg>
      </span>
      <span className="checkbox__label">{children}</span>
    </label>
  )
}
