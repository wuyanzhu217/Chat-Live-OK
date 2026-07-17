import type { IceServerConfig } from '@/types/call'

/** Add explicit UDP/TCP TURN URLs until server rtc-config includes both. */
export function augmentIceServers(servers: IceServerConfig[]): IceServerConfig[] {
  return servers.map((server) => {
    const raw = server.urls
    const urls = (Array.isArray(raw) ? raw : [raw]).filter(Boolean) as string[]
    const merged = new Set<string>(urls)

    for (const url of urls) {
      if (!url.startsWith('turn:')) continue
      const base = url.split('?')[0]
      merged.add(`${base}?transport=udp`)
      merged.add(`${base}?transport=tcp`)
    }

    const next = [...merged]
    return {
      ...server,
      urls: next.length === 1 ? next[0] : next,
    }
  })
}
