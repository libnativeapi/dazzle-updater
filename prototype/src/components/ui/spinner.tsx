import './spinner.css'

/** `NSProgressIndicator` in spinning style; a ring elsewhere. */
export function Spinner({ label }: { label: string }) {
  return (
    <span className="spinner" role="progressbar" aria-label={label}>
      <svg className="spinner__mac" viewBox="0 0 32 32" aria-hidden>
        {Array.from({ length: 12 }, (_, index) => (
          <rect
            key={index}
            x="14.5"
            y="2"
            width="3"
            height="8"
            rx="1.5"
            transform={`rotate(${index * 30} 16 16)`}
            style={{ opacity: 0.15 + (index / 12) * 0.85 }}
          />
        ))}
      </svg>
      <svg className="spinner__ring" viewBox="0 0 32 32" aria-hidden>
        <circle cx="16" cy="16" r="13" />
      </svg>
    </span>
  )
}
