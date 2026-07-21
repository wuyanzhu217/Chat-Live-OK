const MAX_UPLOAD_BYTES = 900 * 1024
const MAX_DIMENSION = 2048

async function loadImageSource(file: File): Promise<ImageBitmap | HTMLImageElement> {
  if (typeof createImageBitmap === 'function') {
    try {
      return await createImageBitmap(file)
    } catch {
      // Fall through for formats createImageBitmap cannot decode (e.g. some HEIC).
    }
  }

  const objectUrl = URL.createObjectURL(file)
  try {
    const img = await new Promise<HTMLImageElement>((resolve, reject) => {
      const el = new Image()
      el.onload = () => resolve(el)
      el.onerror = () => reject(new Error('无法读取图片，请换一张或先在相册中转为 JPEG'))
      el.src = objectUrl
    })
    return img
  } finally {
    URL.revokeObjectURL(objectUrl)
  }
}

function closeImageSource(source: ImageBitmap | HTMLImageElement): void {
  if ('close' in source && typeof source.close === 'function') {
    source.close()
  }
}

/** Resize/compress images so mobile uploads stay under the server body limit (~1MB). */
export async function compressImageForUpload(
  file: File,
  maxBytes = MAX_UPLOAD_BYTES,
): Promise<File> {
  if (!file.type.startsWith('image/')) {
    throw new Error('仅支持上传图片文件')
  }
  if (file.size <= maxBytes) {
    return file
  }

  const source = await loadImageSource(file)
  const srcWidth = 'naturalWidth' in source ? source.naturalWidth : source.width
  const srcHeight = 'naturalHeight' in source ? source.naturalHeight : source.height

  let width = srcWidth
  let height = srcHeight
  const longest = Math.max(width, height)
  if (longest > MAX_DIMENSION) {
    const scale = MAX_DIMENSION / longest
    width = Math.round(width * scale)
    height = Math.round(height * scale)
  }

  const canvas = document.createElement('canvas')
  canvas.width = width
  canvas.height = height
  const ctx = canvas.getContext('2d')
  if (!ctx) {
    closeImageSource(source)
    throw new Error('无法处理图片，请换一张较小的图片')
  }
  ctx.drawImage(source, 0, 0, width, height)
  closeImageSource(source)

  let quality = 0.85
  let blob: Blob | null = null
  while (quality >= 0.45) {
    blob = await new Promise<Blob | null>((resolve) => {
      canvas.toBlob(resolve, 'image/jpeg', quality)
    })
    if (blob && blob.size <= maxBytes) break
    quality -= 0.1
  }

  if (!blob || blob.size > maxBytes) {
    throw new Error('图片过大，请选择较小的图片或在相册中裁剪后再试')
  }

  const baseName = file.name.replace(/\.[^.]+$/, '') || 'image'
  return new File([blob], `${baseName}.jpg`, { type: 'image/jpeg', lastModified: Date.now() })
}
