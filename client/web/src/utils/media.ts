export const MEDIA_SECURE_CONTEXT_HINT =
  '语音/视频需要 HTTPS 或 localhost。请使用 https://服务器IP:8888 访问（注意是 https 不是 http）'

export function isMediaDevicesAvailable(): boolean {
  return (
    typeof navigator !== 'undefined' &&
    typeof navigator.mediaDevices?.getUserMedia === 'function'
  )
}

/** Throws with a user-facing message when mic/camera APIs are unavailable. */
export function assertMediaDevicesAvailable(): void {
  if (isMediaDevicesAvailable()) return
  if (typeof globalThis !== 'undefined' && !globalThis.isSecureContext) {
    throw new Error(MEDIA_SECURE_CONTEXT_HINT)
  }
  throw new Error('无法访问麦克风/摄像头，请检查浏览器权限或设备')
}

export function formatMediaError(err: unknown): string {
  if (!(err instanceof Error)) return '无法启动麦克风/摄像头'

  const { name, message } = err
  if (
    typeof globalThis !== 'undefined' &&
    !globalThis.isSecureContext &&
    (message.includes('getUserMedia') ||
      message.includes('secure') ||
      message.includes('undefined') ||
      message.includes('not supported'))
  ) {
    return MEDIA_SECURE_CONTEXT_HINT
  }
  if (name === 'NotAllowedError' || name === 'PermissionDeniedError') {
    return '麦克风/摄像头权限被拒绝，请在浏览器设置中允许'
  }
  if (name === 'NotFoundError' || name === 'DevicesNotFoundError') {
    return '未检测到麦克风或摄像头设备'
  }
  if (name === 'NotReadableError' || name === 'TrackStartError') {
    return '麦克风/摄像头被其他应用占用'
  }
  return message || '无法启动麦克风/摄像头'
}
