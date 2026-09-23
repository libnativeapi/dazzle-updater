import type { Preview } from '@storybook/react-vite'

import { LocaleContext, LOCALES, resolveLocale } from '../src/features/updater-ui'
import { APPEARANCES, applyTheme, DEFAULT_APPEARANCE, DEFAULT_DESKTOP, DESKTOPS } from '../src/styles/themes'

import '../src/styles/native.css'
import '../src/styles/globals.css'

const preview: Preview = {
  globalTypes: {
    desktop: {
      description: 'Desktop whose native controls are drawn; Omarchy comes as one entry per theme',
      toolbar: {
        title: 'Desktop',
        icon: 'browser',
        items: DESKTOPS.map(desktop => ({ value: desktop.value, title: desktop.title })),
        dynamicTitle: true,
      },
    },
    appearance: {
      description: 'Light or dark appearance',
      toolbar: {
        title: 'Appearance',
        icon: 'contrast',
        items: APPEARANCES.map(appearance => ({ value: appearance.value, title: appearance.title })),
        dynamicTitle: true,
      },
    },
    locale: {
      description: 'Window language',
      toolbar: {
        title: 'Language',
        icon: 'globe',
        items: LOCALES.map(locale => ({ value: locale.value, title: locale.title })),
        dynamicTitle: true,
      },
    },
  },
  initialGlobals: {
    desktop: DEFAULT_DESKTOP,
    appearance: DEFAULT_APPEARANCE,
    locale: 'en',
  },
  decorators: [
    (Story, context) => {
      applyTheme(document.documentElement, String(context.globals.desktop), String(context.globals.appearance))
      const locale = resolveLocale(String(context.globals.locale))
      document.documentElement.lang = locale
      return (
        <LocaleContext.Provider value={locale}>
          <Story />
        </LocaleContext.Provider>
      )
    },
  ],
  parameters: {
    layout: 'centered',
    backgrounds: { disable: true },
    options: {
      storySort: {
        // The host app running whole rounds first; then each window a round is
        // made of, in the order it shows them; then what Sparkle does beyond the spec.
        order: [
          'Introduction',
          'Examples',
          ['Host App'],
          'Components',
          ['Update Available Window', 'Update Status Window', 'Update Permission Window', 'Markdown View'],
          'Reference',
          ['Sparkle'],
        ],
      },
    },
  },
}

export default preview
