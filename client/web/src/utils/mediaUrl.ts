/** Rewrite legacy MinIO HTTP URLs to same-origin HTTPS media proxy (avoids mixed content). */
export function normalizeMediaUrl(url: string | null | undefined): string {
  if (!url) return ''

  try {
    const parsed = new URL(url)
    if (parsed.port === '9000' && parsed.pathname.startsWith('/chatlive-media/')) {
      const origin =
        typeof window !== 'undefined' && window.location.origin
          ? window.location.origin
          : `https://${parsed.hostname}:8888`
      return `${origin}/media${parsed.pathname}${parsed.search}`
    }
  } catch {
    // Keep original URL when parsing fails.
  }

  return url
}
