import { createContext, useContext } from 'react'

import type { ErrorCode } from './types'

/**
 * The window's wording in English and Simplified Chinese — the prototype's
 * copy of `src/updater_ui_strings.*`. The window follows the system's
 * preferred language and falls back to English.
 */
export type Locale = 'en' | 'zh-Hans'

export interface UpdaterUIStrings {
  softwareUpdate: string
  appUpdate: (app: string) => string
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
  errorTitle: { check: string; download: string; install: string }
  /** The Error state's text, by `GetErrorCode()`; `other` for anything else. */
  errorText: (app: string) => Partial<Record<ErrorCode, string>> & { other: string }
  skip: string
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
  cancel: string
  later: string
  installUpdate: string
  installAndRelaunch: string
  ok: string
}

export const STRINGS: Record<Locale, UpdaterUIStrings> = {
  en: {
    softwareUpdate: 'Software Update',
    appUpdate: app => `${app} Update`,
    checking: 'Checking for updates…',
    availableTitle: app => `A new version of ${app} is available!`,
    availableDetail: (app, version, current) => `${app} ${version} is now available—you have ${current}.`,
    viewReleaseNotes: 'View Release Notes',
    downloading: 'Downloading update…',
    downloadedOf: (received, total) => `${received} of ${total}`,
    downloaded: received => `${received} downloaded`,
    readyTitle: 'Ready to Install',
    installing: 'Installing…',
    upToDateTitle: 'You’re up to date!',
    upToDateDetail: (app, current) => `${app} ${current} is currently the newest version available.`,
    errorTitle: {
      check: 'An error occurred while checking for updates.',
      download: 'An error occurred while downloading the update.',
      install: 'An error occurred while installing the update.',
    },
    cancel: 'Cancel',
    later: 'Later',
    installUpdate: 'Install Update',
    installAndRelaunch: 'Install and Relaunch',
    ok: 'OK',
    errorText: app => ({
      network: 'Couldn’t connect to the update server. Check your internet connection and try again.',
      'invalid-index': 'The update information isn’t in the expected format. Try again later, or contact the developer.',
      signature: 'The update isn’t signed correctly, so it wasn’t installed. Try again later, or contact the developer.',
      permission: `${app} can’t be replaced. Allow ${app} to modify apps in System Settings › Privacy & Security › App Management, or use an account that can.`,
      'app-location': `${app} is running from a read-only or temporary location and can’t be updated there. Move it to the Applications folder, open it from there and try again.`,
      installer: 'The installer couldn’t be started. Try again later.',
      other: 'An unknown error occurred. Try again later.',
    }),
    skip: 'Skip This Version',
    automaticDownloads: 'Automatically download and install updates in the future',
    downloadedTitle: app => `A new version of ${app} is ready to install!`,
    downloadedDetail: (app, version) => `${app} ${version} has been downloaded. Would you like to install it and relaunch ${app} now?`,
    permissionTitle: 'Check for updates automatically?',
    permissionDetail: app => `${app} can check for new versions automatically. You can always check for updates manually from the ${app} menu.`,
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
      appleSiliconDetail: (app, version) => `${app} ${version} is available but this update requires a new Apple silicon Mac.`,
      installedTitle: 'Update Installed',
      installedDetail: (app, version) => `${app} is now updated to version ${version}!`,
    },
  },
  'zh-Hans': {
    softwareUpdate: '软件更新',
    appUpdate: app => `${app} 更新`,
    checking: '正在检查更新…',
    availableTitle: app => `${app} 有新版本可用`,
    availableDetail: (app, version, current) => `${app} ${version} 已发布，你当前使用的是 ${current}。`,
    viewReleaseNotes: '查看更新说明',
    downloading: '正在下载更新…',
    downloadedOf: (received, total) => `已下载 ${received} / ${total}`,
    downloaded: received => `已下载 ${received}`,
    readyTitle: '更新已准备就绪',
    installing: '正在安装…',
    upToDateTitle: '已是最新版本',
    upToDateDetail: (app, current) => `${app} ${current} 已是最新版本。`,
    errorTitle: {
      check: '检查更新时出错',
      download: '下载更新时出错',
      install: '安装更新时出错',
    },
    cancel: '取消',
    later: '以后再说',
    installUpdate: '安装更新',
    installAndRelaunch: '安装并重新启动',
    ok: '好',
    errorText: app => ({
      network: '无法连接到更新服务器。请检查网络连接后再试。',
      'invalid-index': '更新信息的格式不正确。请稍后再试，或联系开发者。',
      signature: '更新包的签名无效，已停止安装。请稍后再试，或联系开发者。',
      permission: `没有权限替换 ${app}。请在"系统设置 → 隐私与安全性 → App 管理"中允许 ${app} 修改应用，或以有权限的账户运行。`,
      'app-location': `${app} 正在从只读或临时位置运行，无法原地更新。请将它移到"应用程序"文件夹，从那里重新打开后再试。`,
      installer: '无法启动安装程序。请稍后再试。',
      other: '发生了未知错误。请稍后再试。',
    }),
    skip: '跳过此版本',
    automaticDownloads: '以后自动下载并安装更新',
    downloadedTitle: app => `${app} 的新版本已准备好安装`,
    downloadedDetail: (app, version) => `${app} ${version} 已下载完成。要现在安装并重新启动 ${app} 吗？`,
    permissionTitle: '要自动检查更新吗？',
    permissionDetail: app => `${app} 可以自动检查新版本。你随时可以在 ${app} 菜单中手动检查更新。`,
    permissionDownloads: '自动下载并安装更新',
    dontCheck: '不检查',
    checkAutomatically: '自动检查',
    reference: {
      criticalTitle: app => `${app} 有一个重要更新`,
      criticalDetail: (app, version, current) => `${app} ${version} 已发布，你当前使用的是 ${current}。这是一个重要更新，要现在下载吗？`,
      informationDetail: (app, version, current) => `${app} ${version} 已发布，你当前使用的是 ${current}。要在网页上了解这次更新吗？`,
      learnMore: '了解更多…',
      notesFailed: '下载更新说明时出错。',
      versionHistory: '版本历史',
      tooOldTitle: '你的 macOS 版本太旧',
      tooOldDetail: (app, version, required) => `${app} ${version} 已发布，但你的 macOS 版本太旧，无法安装。至少需要 macOS ${required}。`,
      tooNewTitle: '你的 macOS 版本太新',
      tooNewDetail: (app, version, required) => `${app} ${version} 已发布，但它不支持你的 macOS 版本。此更新最高只支持 macOS ${required}。`,
      appleSiliconTitle: '你的 Mac 太旧',
      appleSiliconDetail: (app, version) => `${app} ${version} 已发布，但此更新需要搭载 Apple 芯片的 Mac。`,
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
