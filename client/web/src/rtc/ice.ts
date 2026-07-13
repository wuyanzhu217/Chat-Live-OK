export function waitIceGathering(pc: RTCPeerConnection, timeoutMs = 5000): Promise<void> {
  if (pc.iceGatheringState === 'complete') return Promise.resolve()
  return new Promise((resolve) => {
    const done = () => {
      pc.removeEventListener('icegatheringstatechange', onChange)
      clearTimeout(timer)
      resolve()
    }
    const onChange = () => {
      if (pc.iceGatheringState === 'complete') done()
    }
    pc.addEventListener('icegatheringstatechange', onChange)
    const timer = setTimeout(done, timeoutMs)
  })
}

export function waitIceConnected(pc: RTCPeerConnection, timeoutMs = 10000): Promise<void> {
  if (pc.iceConnectionState === 'connected' || pc.iceConnectionState === 'completed') {
    return Promise.resolve()
  }
  return new Promise((resolve, reject) => {
    const cleanup = () => {
      pc.removeEventListener('iceconnectionstatechange', onChange)
      clearTimeout(timer)
    }
    const onChange = () => {
      const state = pc.iceConnectionState
      if (state === 'connected' || state === 'completed') {
        cleanup()
        resolve()
      } else if (state === 'failed' || state === 'closed') {
        cleanup()
        reject(new Error(`ICE ${state}`))
      }
    }
    pc.addEventListener('iceconnectionstatechange', onChange)
    const timer = setTimeout(() => {
      cleanup()
      reject(new Error(`ICE timeout (${pc.iceConnectionState})`))
    }, timeoutMs)
  })
}
