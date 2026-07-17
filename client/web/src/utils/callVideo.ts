/** Prime <video> elements during a user gesture (required on iOS Safari). */
export function primeCallVideoPlayback(): void {
  for (const id of ['call-remote-video', 'call-local-video'] as const) {
    const el = document.getElementById(id) as HTMLVideoElement | null
    if (!el) continue
    el.muted = id === 'call-local-video'
    el.playsInline = true
    // Must run synchronously inside click/tap handler — do not await before calling.
    void el.play().catch(() => {})
  }
}

export async function bindCallVideoElement(
  el: HTMLVideoElement | null,
  stream: MediaStream | null,
  muted: boolean,
): Promise<void> {
  if (!el) return
  if (el.srcObject !== stream) {
    el.srcObject = stream
  }
  el.muted = muted
  el.playsInline = true
  if (!stream) return
  try {
    await el.play()
  } catch (e) {
    console.warn('[callVideo] video.play blocked', e)
    try {
      el.muted = true
      await el.play()
      if (!muted) el.muted = false
    } catch (e2) {
      console.warn('[callVideo] video.play retry failed', e2)
    }
  }
}
