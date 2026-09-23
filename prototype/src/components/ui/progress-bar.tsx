import './progress-bar.css'

export interface ProgressBarProps {
  /** 0–1. Left out, the bar is indeterminate and animates. */
  value?: number
  label: string
}

/** `NSProgressIndicator` in bar style; the Windows `ProgressBar`. */
export function ProgressBar({ value, label }: ProgressBarProps) {
  const indeterminate = value === undefined
  return (
    <div
      className="progress-bar"
      role="progressbar"
      aria-label={label}
      aria-valuemin={0}
      aria-valuemax={100}
      aria-valuenow={indeterminate ? undefined : Math.round(value * 100)}
      data-indeterminate={indeterminate ? '' : undefined}
    >
      <div className="progress-bar__fill" style={indeterminate ? undefined : { width: `${Math.min(1, value) * 100}%` }} />
    </div>
  )
}
