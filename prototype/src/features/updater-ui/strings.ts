import { createContext, useContext } from 'react'

import type { ErrorCode } from './types'

/**
 * The window's wording, taken from Sparkle 2.x rather than invented: the strings are
 * Sparkle's own, English from `Sparkle/Base.lproj/Sparkle.strings` and Simplified Chinese
 * from `Sparkle/zh_CN.lproj/Sparkle.strings`, both at the 2.x tip.
 *
 * The prototype's copy of `src/updater_ui_strings.*`. The window follows the system's
 * preferred language and falls back to English.
 */
export type Locale = 'en' | 'zh-Hans'

export interface UpdaterUIStrings {
  /** Both windows are titled this while nothing has been found yet. */
  softwareUpdate: string
  /** The status window's title once an update is being downloaded or installed. */
  updating: (app: string) => string
  checking: string
  availableTitle: (app: string) => string
  availableDetail: (app: string, version: string, current: string) => string
  viewReleaseNotes: string
  downloading: string
  downloadedOf: (received: string, total: string) => string
  downloaded: (received: string) => string
  readyTitle: string
  installing: string
  upToDateTitle: string
  upToDateDetail: (app: string, current: string) => string
  /** Sparkle titles every update failure "Update Error!" and explains it below. */
  errorTitle: string
  /** The Error state's text, by `GetErrorCode()`; `other` for anything else. */
  errorText: (app: string) => Partial<Record<ErrorCode, string>> & { other: string }
  skip: string
  later: string
  installUpdate: string
  installAndRelaunch: string
  cancel: string
  ok: string
  automaticDownloads: string
  downloadedTitle: (app: string) => string
  downloadedDetail: (app: string, version: string) => string
  permissionTitle: string
  permissionDetail: (app: string) => string
  permissionDownloads: string
  dontCheck: string
  checkAutomatically: string
  /** Sparkle's wording for what the spec has not taken up yet (Sparkle Reference stories). */
  reference: {
    criticalTitle: (app: string) => string
    criticalDetail: (app: string, version: string, current: string) => string
    informationDetail: (app: string, version: string, current: string) => string
    learnMore: string
    notesFailed: string
    versionHistory: string
    tooOldTitle: string
    tooOldDetail: (app: string, version: string, required: string) => string
    tooNewTitle: string
    tooNewDetail: (app: string, version: string, required: string) => string
    appleSiliconTitle: string
    appleSiliconDetail: (app: string, version: string) => string
    installedTitle: string
    installedDetail: (app: string, version: string) => string
  }
}

export const STRINGS: Record<Locale, UpdaterUIStrings> = {
  en: {
    softwareUpdate: 'Software Update',
    updating: app => `Updating ${app}`,
    checking: 'Checking for updates…',
    availableTitle: app => `A new version of ${app} is available!`,
    availableDetail: (app, version, current) =>
      `${app} ${version} is now available—you have ${current}. Would you like to download it now?`,
    viewReleaseNotes: 'View Release Notes',
    downloading: 'Downloading update…',
    downloadedOf: (received, total) => `${received} of ${total}`,
    downloaded: received => `${received} downloaded`,
    readyTitle: 'Ready to Install',
    installing: 'Installing update…',
    upToDateTitle: 'You’re up to date!',
    upToDateDetail: (app, current) => `${app} ${current} is currently the newest version available.`,
    errorTitle: 'Update Error!',
    errorText: app => ({
      network: 'An error occurred in retrieving update information. Please try again later.',
      'invalid-index': 'An error occurred while parsing the update feed.',
      signature:
        'The update is improperly signed and could not be validated. Please try again later or contact the app developer.',
      permission: `If ${app} is running from the location it was downloaded to, it can’t be updated.`,
      'app-location': `${app} can’t be updated if it’s running from the location it was downloaded to.`,
      installer: 'An error occurred while launching the installer. Please try again later.',
      other: 'An error occurred while running the updater. Please try again later.',
    }),
    skip: 'Skip This Version',
    later: 'Remind Me Later',
    installUpdate: 'Install Update',
    installAndRelaunch: 'Install and Relaunch',
    cancel: 'Cancel',
    ok: 'OK',
    automaticDownloads: 'Automatically download and install updates in the future',
    downloadedTitle: app => `A new version of ${app} is ready to install!`,
    downloadedDetail: (app, version) =>
      `${app} ${version} has been downloaded and is ready to use! Would you like to install it and relaunch ${app} now?`,
    permissionTitle: 'Check for updates automatically?',
    permissionDetail: app =>
      `Should ${app} automatically check for updates? You can always check for updates manually from the ${app} menu.`,
    permissionDownloads: 'Automatically download and install updates',
    dontCheck: 'Don’t Check',
    checkAutomatically: 'Check Automatically',
    reference: {
      criticalTitle: app => `An important update to ${app} is ready to install`,
      criticalDetail: (app, version, current) =>
        `${app} ${version} is now available—you have ${current}. This is an important update; would you like to download it now?`,
      informationDetail: (app, version, current) =>
        `${app} ${version} is now available—you have ${current}. Would you like to learn more about this update on the web?`,
      learnMore: 'Learn More…',
      notesFailed: 'An error occurred while downloading the release notes.',
      versionHistory: 'Version History',
      tooOldTitle: 'Your macOS version is too old',
      tooOldDetail: (app, version, required) =>
        `${app} ${version} is available but your macOS version is too old to install it. At least macOS ${required} is required.`,
      tooNewTitle: 'Your macOS version is too new',
      tooNewDetail: (app, version, required) =>
        `${app} ${version} is available but your macOS version is too new for this update. This update only supports up to macOS ${required}.`,
      appleSiliconTitle: 'Your Mac is too old',
      appleSiliconDetail: (app, version) =>
        `${app} ${version} is available but this update requires a new Apple silicon Mac.`,
      installedTitle: 'Update Installed',
      installedDetail: (app, version) => `${app} is now updated to version ${version}!`,
    },
  },
  'zh-Hans': {
    softwareUpdate: '软件更新',
    updating: app => `正在更新${app}`,
    checking: '正在检查更新…',
    availableTitle: app => `新版本的${app}已经发布`,
    availableDetail: (app, version, current) =>
      `${app} ${version}可供下载，您现在的版本是${current}。要现在下载吗？`,
    viewReleaseNotes: '查看更新说明',
    downloading: '正在下载更新…',
    downloadedOf: (received, total) => `${received} / ${total}`,
    downloaded: received => `${received}已下载`,
    readyTitle: '可以开始安装了',
    installing: '正在安装更新…',
    upToDateTitle: '您使用的就是最新版！',
    upToDateDetail: (app, current) => `${app} ${current}是当前的最新版本。`,
    errorTitle: '更新错误！',
    errorText: app => ({
      network: '获取升级信息时出现错误，请稍后再试。',
      'invalid-index': '解析更新信息时出现错误，请稍后再试。',
      signature: '此更新未正确签名，无法验证其真实性。请稍后再试或联系App开发者。',
      permission: `如果在下载位置运行此程序，则无法对${app}进行更新。`,
      'app-location': `如果在下载位置运行此程序，则无法对${app}进行更新。`,
      installer: '打开安装程序时出现错误，请稍后再试。',
      other: '运行更新程序时出现错误，请稍后再试。',
    }),
    skip: '跳过这个版本',
    later: '稍后提醒我',
    installUpdate: '安装更新',
    installAndRelaunch: '安装并重启应用',
    cancel: '取消',
    ok: '好',
    automaticDownloads: '以后自动下载并安装更新',
    downloadedTitle: app => `新版本的${app}可以安装了`,
    downloadedDetail: (app, version) =>
      `${app} ${version}已下载完毕并可以使用。要立刻安装并重启${app}吗？`,
    permissionTitle: '自动检查更新？',
    permissionDetail: app =>
      `您想让${app}在启动时自动检查更新么？您也可以在程序菜单${app}中手动检查。`,
    permissionDownloads: '自动下载并安装更新',
    dontCheck: '不检查',
    checkAutomatically: '自动检查',
    reference: {
      criticalTitle: app => `已准备好安装${app}的重要更新`,
      criticalDetail: (app, version, current) =>
        `${app} ${version}可供下载，您现在的版本是${current}。这是重要更新，要现在下载吗？`,
      informationDetail: (app, version, current) =>
        `${app} ${version}可供下载，您现在的版本是${current}。您要在网站上查看关于本次更新的更多信息吗？`,
      learnMore: '查看更多…',
      notesFailed: '下载更新说明时出现错误。',
      versionHistory: '版本历史记录',
      tooOldTitle: '您的macOS版本太旧了',
      tooOldDetail: (app, version, required) =>
        `${app} ${version}可供下载，但您的macOS版本因太旧而无法安装。最低版本要求为macOS ${required}。`,
      tooNewTitle: '您的macOS版本太新了',
      tooNewDetail: (app, version, required) =>
        `${app} ${version}可供下载，但您的macOS版本因太新而无法安装本次更新。本次更新仅支持至macOS ${required}。`,
      appleSiliconTitle: '你的 Mac 太旧',
      appleSiliconDetail: (app, version) =>
        `${app} ${version} 已发布，但此更新需要搭载 Apple 芯片的 Mac。`,
      installedTitle: '更新已安装',
      installedDetail: (app, version) => `${app} 已更新到 ${version} 版！`,
    },
  },
}

export const LOCALES = [
  { value: 'en', title: 'English' },
  { value: 'zh-Hans', title: '简体中文' },
] as const

/** A toolbar value as a locale; anything unknown falls back to English, as the window does. */
export function resolveLocale(value: string): Locale {
  return value === 'zh-Hans' ? 'zh-Hans' : 'en'
}

export const LocaleContext = createContext<Locale>('en')

export function useStrings(): UpdaterUIStrings {
  return STRINGS[useContext(LocaleContext)]
}
