import type { ComponentProps } from 'react'

import './button.css'

export interface ButtonProps extends ComponentProps<'button'> {
  /** The default button: accent-filled, and what Return presses. */
  isDefault?: boolean
}

/** A push button with the platform's rounded bezel. */
export function Button({ isDefault = false, className, type = 'button', ...props }: ButtonProps) {
  return (
    <button
      type={type}
      data-default={isDefault ? '' : undefined}
      className={['button', className].filter(Boolean).join(' ')}
      {...props}
    />
  )
}
