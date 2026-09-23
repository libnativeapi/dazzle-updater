import { useEffect, useRef, useState, type CSSProperties, type PointerEvent as ReactPointerEvent, type ReactNode } from 'react'

import { AppIcon } from '../../../components/ui'

import './desktop.css'

/** One of UpdaterUI's windows, and which one it is. */
export interface DesktopWindow {
  /** The window's identity: the same key keeps the window and only updates its content. */
  key: 'available' | 'status' | 'permission'
  node: ReactNode
}

/** How long the old window stays under the new one before it closes. */
const HANDOVER_MS = 160

/** What a drag that starts at `target` does: move the window, resize it from an edge, or nothing. */
function dragKind(target: HTMLElement): { kind: 'move' } | { kind: 'resize'; edge: string } | null {
  const edge = target.closest<HTMLElement>('.window__resize')
  if (edge) return { kind: 'resize', edge: edge.dataset.edge ?? '' }
  if (target.closest('.window__titlebar')) return { kind: 'move' }
  // movableByWindowBackground: any part of the window that is not a control or text.
  if (target.closest('.window[data-movable-background]') && !target.closest('button, a, [aria-label="Release notes"]')) {
    return { kind: 'move' }
  }
  return null
}

/**
 * The screen UpdaterUI's windows stand on, and the rules for moving between
 * them (specs/updater-ui.md, "窗口与流程"):
 *
 * - at most one window at a time;
 * - switching windows shows the new one first, centred on the old one's centre,
 *   then closes the old one — so it reads as a replacement in place;
 * - a state change inside the same window only updates its content.
 *
 * It also does what the window server does for any window: dragging by the
 * titlebar (or the background, where the window allows it), resizing from the
 * edges down to the window's minimum — a size remembered for the next time
 * that window opens, as Sparkle's frame autosave does — and key status: a click
 * elsewhere — the desktop, the host app — makes the window inactive, a click in
 * it makes it key again.
 * `children` — the host app — stands behind UpdaterUI's windows the whole time.
 */
export function Desktop({
  window: current,
  dock,
  children,
}: {
  window: DesktopWindow | null
  /**
   * The macOS Dock with the app in it. `attention` bounces its icon once, as
   * `requestUserAttention:` does — Sparkle's cue when an update is ready.
   */
  dock?: { attention: boolean }
  children: ReactNode
}) {
  // The centre every window is placed on, as an offset from the screen's centre.
  const [center, setCenter] = useState({ x: 0, y: 0 })
  // Content sizes the reader has resized a window to, by window.
  const [sizes, setSizes] = useState<Partial<Record<DesktopWindow['key'], { width: number; height: number }>>>({})
  const [active, setActive] = useState(true)
  const [outgoing, setOutgoing] = useState<DesktopWindow | null>(null)
  const shown = useRef<DesktopWindow | null>(null)

  const previous = shown.current
  if (previous && current && previous.key !== current.key && outgoing?.key !== previous.key) {
    // Switching windows: the old one stays, under the new, until the handover ends.
    setOutgoing(previous)
  }

  useEffect(() => {
    // A window that opens is the key window.
    if (current && current.key !== shown.current?.key) setActive(true)
    // A window put away does not remember where it was: the next one opens
    // centred. Only on the render that puts it away — this effect runs after
    // every render, and a fresh object each time would render forever.
    if (!current && shown.current) setCenter(value => (value.x === 0 && value.y === 0 ? value : { x: 0, y: 0 }))
    shown.current = current
  })

  useEffect(() => {
    if (!outgoing) return
    const timer = setTimeout(() => setOutgoing(null), HANDOVER_MS)
    return () => clearTimeout(timer)
  }, [outgoing])

  function onPointerDown(event: ReactPointerEvent<HTMLDivElement>) {
    const target = event.target as HTMLElement
    const inWindow = target.closest('.desktop__window:not([data-closing])')
    setActive(Boolean(inWindow))
    if (!inWindow || !current || event.button !== 0) return
    const drag = dragKind(target)
    if (!drag) return
    event.preventDefault()

    const frame = inWindow.querySelector<HTMLElement>('.window')
    const content = inWindow.querySelector<HTMLElement>('.window__content')
    const minWidth = Number(frame?.dataset.minWidth ?? 0)
    const minHeight = Number(frame?.dataset.minHeight ?? 0)
    // The size as the CSS states it — the value --resized-* will be set to —
    // so a content box with padding does not grow by it on every drag.
    const styled = content ? getComputedStyle(content) : null
    let size = { width: parseFloat(styled?.width ?? '0'), height: parseFloat(styled?.height ?? '0') }
    const key = current.key

    const surface = event.currentTarget
    surface.setPointerCapture(event.pointerId)
    let last = { x: event.clientX, y: event.clientY }
    const move = (moveEvent: PointerEvent) => {
      const dx = moveEvent.clientX - last.x
      const dy = moveEvent.clientY - last.y
      last = { x: moveEvent.clientX, y: moveEvent.clientY }
      if (drag.kind === 'move') {
        setCenter(value => ({ x: value.x + dx, y: value.y + dy }))
        return
      }
      // Resizing keeps the opposite edge where it is, so the centre moves by half the change.
      const edge = drag.edge
      const sign = { x: edge.includes('e') ? 1 : edge.includes('w') ? -1 : 0, y: edge.includes('s') ? 1 : edge.includes('n') ? -1 : 0 }
      const next = {
        width: Math.max(minWidth, size.width + sign.x * dx),
        height: Math.max(minHeight, size.height + sign.y * dy),
      }
      const grew = { x: next.width - size.width, y: next.height - size.height }
      if (grew.x === 0 && grew.y === 0) return
      size = next
      setSizes(value => ({ ...value, [key]: next }))
      setCenter(value => ({ x: value.x + (sign.x * grew.x) / 2, y: value.y + (sign.y * grew.y) / 2 }))
    }
    const up = () => {
      surface.removeEventListener('pointermove', move)
      surface.removeEventListener('pointerup', up)
    }
    surface.addEventListener('pointermove', move)
    surface.addEventListener('pointerup', up)
  }

  const placed = { left: `calc(50% + ${center.x}px)`, top: `calc(50% + ${center.y}px)` }
  const resized = current && sizes[current.key]

  return (
    <div className="desktop" onPointerDown={onPointerDown}>
      {/* The host app is key only while no update window is, or when clicked. */}
      <div className="desktop__host" data-inactive={current && active ? '' : undefined}>
        {children}
      </div>
      {outgoing && current && (
        <div key={`closing-${outgoing.key}`} className="desktop__window" data-closing="" style={placed} aria-hidden>
          {outgoing.node}
        </div>
      )}
      {current && (
        <div
          key={current.key}
          className="desktop__window"
          data-inactive={active ? undefined : ''}
          style={
            {
              ...placed,
              ...(resized && { '--resized-width': `${resized.width}px`, '--resized-height': `${resized.height}px` }),
            } as CSSProperties
          }
        >
          {current.node}
        </div>
      )}
      {dock && (
        <div className="desktop__dock" aria-hidden>
          <span className="desktop__dock-app" data-attention={dock.attention ? '' : undefined}>
            <AppIcon size={48} />
          </span>
          <span className="desktop__dock-running" />
        </div>
      )}
    </div>
  )
}
