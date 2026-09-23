import type { CSSProperties, ReactNode } from 'react'

import './window.css'

export interface WindowControls {
  /** `false` draws the control disabled (macOS) or leaves it out (Windows). */
  close?: boolean
  minimize?: boolean
  zoom?: boolean
}

export interface WindowProps {
  /** The window's title. Kept for accessibility even when not drawn. */
  title: string
  /** `NSWindow.titleVisibility = hidden` with a transparent titlebar. */
  titleHidden?: boolean
  controls?: WindowControls
  /** Content width in points, as `contentRect` states it. */
  width: number
  /** Content height in points; left out, the window sizes to its content. */
  height?: number
  /**
   * `NSWindow.movableByWindowBackground`: the window can be dragged from any
   * empty part of its background, not only its titlebar.
   */
  movableByBackground?: boolean
  /**
   * Resizable from every edge and corner, down to this content size. The
   * surface the window stands on does the resizing: the edges only mark where
   * it can start, and `--resized-width` / `--resized-height` carry the result.
   */
  resizable?: { minWidth: number; minHeight: number }
  /**
   * Whether the window has lost key status. An inactive one greys its controls
   * and title; a parent element carrying `data-inactive` does the same.
   */
  inactive?: boolean
  className?: string
  style?: CSSProperties
  children: ReactNode
}

/**
 * A top-level window: the titlebar with the platform's window controls, and the
 * content view below it. On macOS the controls are the traffic lights on the
 * leading edge; on Windows the caption buttons on the trailing edge; on Linux
 * the desktop's own headerbar buttons on the trailing edge — Close alone on
 * GNOME, all three on Ubuntu and KDE — and none at all under Omarchy's tiling
 * compositor, which draws no titlebar.
 */
export function Window({
  title,
  titleHidden = false,
  controls = { close: true, minimize: true, zoom: true },
  width,
  height,
  movableByBackground = false,
  resizable,
  inactive = false,
  className,
  style,
  children,
}: WindowProps) {
  return (
    <div
      role="dialog"
      aria-label={title}
      data-inactive={inactive ? '' : undefined}
      data-movable-background={movableByBackground ? '' : undefined}
      data-min-width={resizable?.minWidth}
      data-min-height={resizable?.minHeight}
      className={['window', className].filter(Boolean).join(' ')}
      style={
        {
          '--content-width': resizable ? `var(--resized-width, ${width}px)` : `${width}px`,
          '--content-height': resizable ? `var(--resized-height, ${height ? `${height}px` : 'auto'})` : height ? `${height}px` : 'auto',
          ...style,
        } as CSSProperties
      }
    >
      {resizable &&
        (['n', 's', 'e', 'w', 'ne', 'nw', 'se', 'sw'] as const).map(edge => (
          <span key={edge} className="window__resize" data-edge={edge} aria-hidden />
        ))}
      <div className="window__titlebar" data-title-hidden={titleHidden ? '' : undefined}>
        <TrafficLights controls={controls} />
        <span className="window__title">{title}</span>
        <CaptionButtons controls={controls} />
        <HeaderbarButtons controls={controls} />
      </div>
      <div className="window__content">{children}</div>
    </div>
  )
}

/** The traffic lights. Hovering the group shows each enabled light's glyph, as AppKit does. */
function TrafficLights({ controls }: { controls: WindowControls }) {
  return (
    <div className="window__traffic" aria-hidden>
      <span data-light="close" data-disabled={controls.close ? undefined : ''}>
        <svg viewBox="0 0 12 12"><path d="M3.5 3.5l5 5M8.5 3.5l-5 5" /></svg>
      </span>
      <span data-light="minimize" data-disabled={controls.minimize ? undefined : ''}>
        <svg viewBox="0 0 12 12"><path d="M2.8 6h6.4" /></svg>
      </span>
      <span data-light="zoom" data-disabled={controls.zoom ? undefined : ''}>
        <svg viewBox="0 0 12 12"><path d="M6 2.8v6.4M2.8 6h6.4" /></svg>
      </span>
    </div>
  )
}

/** Windows caption buttons. A window that cannot be minimised or maximised shows Close alone. */
function CaptionButtons({ controls }: { controls: WindowControls }) {
  const sizing = controls.minimize || controls.zoom
  return (
    <div className="window__caption" aria-hidden>
      {sizing && (
        <>
          <span data-caption="minimize" data-disabled={controls.minimize ? undefined : ''}>
            <svg viewBox="0 0 10 10"><path d="M0 5.5h10" /></svg>
          </span>
          <span data-caption="maximize" data-disabled={controls.zoom ? undefined : ''}>
            <svg viewBox="0 0 10 10"><rect x="0.5" y="0.5" width="9" height="9" rx="1" /></svg>
          </span>
        </>
      )}
      <span data-caption="close" data-disabled={controls.close ? undefined : ''}>
        <svg viewBox="0 0 10 10"><path d="M0.5 0.5l9 9M9.5 0.5l-9 9" /></svg>
      </span>
    </div>
  )
}

/** Linux headerbar buttons. Which of them a desktop shows is decided in `window.css`. */
function HeaderbarButtons({ controls }: { controls: WindowControls }) {
  return (
    <div className="window__headerbar" aria-hidden>
      {controls.minimize && (
        <span data-headerbar="minimize">
          <svg viewBox="0 0 16 16" data-glyph="gnome"><path d="M4.5 11.5h7" /></svg>
          <svg viewBox="0 0 16 16" data-glyph="kde"><path d="M4 6l4 4 4-4" /></svg>
        </span>
      )}
      {controls.zoom && (
        <span data-headerbar="maximize">
          <svg viewBox="0 0 16 16" data-glyph="gnome"><rect x="4.5" y="4.5" width="7" height="7" rx="1" /></svg>
          <svg viewBox="0 0 16 16" data-glyph="kde"><path d="M4 10l4-4 4 4" /></svg>
        </span>
      )}
      <span data-headerbar="close" data-disabled={controls.close ? undefined : ''}>
        <svg viewBox="0 0 16 16" data-glyph="gnome"><path d="M5 5l6 6M11 5l-6 6" /></svg>
        <svg viewBox="0 0 16 16" data-glyph="kde"><path d="M4.5 4.5l7 7M11.5 4.5l-7 7" /></svg>
      </span>
    </div>
  )
}
