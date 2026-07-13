import { apiClient } from './client'

export interface UploadImageResult {
  media_url: string
  thumbnail_url: string | null
}

export function uploadImage(file: File) {
  const form = new FormData()
  form.append('file', file)
  return apiClient.uploadForm<UploadImageResult>('/uploads/images', form)
}
