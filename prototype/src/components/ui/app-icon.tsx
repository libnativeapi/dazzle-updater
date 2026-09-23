import { useId } from 'react'

/**
 * The host application's icon — what `NSApplicationIcon` is in Sparkle's
 * windows. A stand-in for the app's own artwork, drawn to the macOS icon grid:
 * a rounded square inset from the canvas edge.
 */
export function AppIcon({ size = 64 }: { size?: number }) {
  const id = useId()
  return (
    <svg width={size} height={size} viewBox="0 0 64 64" aria-hidden style={{ flexShrink: 0 }}>
      <defs>
        <linearGradient id={`${id}-bg`} x1="0" y1="0" x2="0" y2="1">
          <stop offset="0" stopColor="#8b7bff" />
          <stop offset="1" stopColor="#4b39d6" />
        </linearGradient>
        <linearGradient id={`${id}-mark`} x1="0" y1="0" x2="0" y2="1">
          <stop offset="0" stopColor="#ffffff" />
          <stop offset="1" stopColor="#dcd6ff" />
        </linearGradient>
        <filter id={`${id}-shadow`} x="-20%" y="-20%" width="140%" height="140%">
          <feDropShadow dx="0" dy="0.8" stdDeviation="0.8" floodOpacity="0.3" />
        </filter>
      </defs>
      <rect x="6" y="6" width="52" height="52" rx="11.6" fill={`url(#${id}-bg)`} filter={`url(#${id}-shadow)`} />
      <path
        d="M32 15c1.4 8.7 7.3 14.6 16 16-8.7 1.4-14.6 7.3-16 16-1.4-8.7-7.3-14.6-16-16 8.7-1.4 14.6-7.3 16-16z"
        fill={`url(#${id}-mark)`}
      />
    </svg>
  )
}
