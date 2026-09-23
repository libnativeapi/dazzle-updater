import { useEffect } from 'react'

/** Return presses the window's default button; Esc is its Later / Cancel. */
export function useWindowKeys(defaultAction: (() => void) | undefined, cancel: (() => void) | undefined) {
  useEffect(() => {
    function onKeyDown(event: KeyboardEvent) {
      if (event.key === 'Enter' && defaultAction) defaultAction()
      else if (event.key === 'Escape' && cancel) cancel()
      else return
      event.preventDefault()
    }
    window.addEventListener('keydown', onKeyDown)
    return () => window.removeEventListener('keydown', onKeyDown)
  }, [defaultAction, cancel])
}
