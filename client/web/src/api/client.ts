import { env } from '@/config/env'
import { ApiError, type ApiResponse, type PageData } from '@/types/api'
import { getAccessToken } from '@/auth/tokenStorage'

type UnauthorizedHandler = () => Promise<boolean>

class ApiClient {
  private unauthorizedHandler: UnauthorizedHandler | null = null

  setUnauthorizedHandler(handler: UnauthorizedHandler): void {
    this.unauthorizedHandler = handler
  }

  private url(path: string): string {
    const base = env.apiBase.replace(/\/$/, '')
    const p = path.startsWith('/') ? path : `/${path}`
    return `${base}${p}`
  }

  private async parseResponse<T>(res: Response): Promise<T> {
    const json = (await res.json()) as ApiResponse<T>
    if (json.code !== 0) {
      throw new ApiError(json.code, json.message || 'Request failed', res.status, json.request_id)
    }
    return json.data
  }

  async request<T>(
    method: string,
    path: string,
    options?: { body?: unknown; auth?: boolean; retry?: boolean },
  ): Promise<T> {
    const auth = options?.auth !== false
    const headers: Record<string, string> = { Accept: 'application/json' }
    if (options?.body !== undefined) {
      headers['Content-Type'] = 'application/json'
    }
    if (auth) {
      const token = getAccessToken()
      if (token) headers.Authorization = `Bearer ${token}`
    }

    const res = await fetch(this.url(path), {
      method,
      headers,
      body: options?.body !== undefined ? JSON.stringify(options.body) : undefined,
    })

    if (res.status === 401 && auth && options?.retry !== false && this.unauthorizedHandler) {
      const refreshed = await this.unauthorizedHandler()
      if (refreshed) {
        return this.request<T>(method, path, { ...options, retry: false })
      }
    }

    return this.parseResponse<T>(res)
  }

  get<T>(path: string): Promise<T> {
    return this.request<T>('GET', path)
  }

  post<T>(path: string, body?: unknown): Promise<T> {
    return this.request<T>('POST', path, { body })
  }

  put<T>(path: string, body?: unknown): Promise<T> {
    return this.request<T>('PUT', path, { body })
  }

  async uploadForm<T>(path: string, formData: FormData): Promise<T> {
    const token = getAccessToken()
    const headers: Record<string, string> = {}
    if (token) headers.Authorization = `Bearer ${token}`

    const res = await fetch(this.url(path), {
      method: 'POST',
      headers,
      body: formData,
    })

    if (res.status === 401 && this.unauthorizedHandler) {
      const refreshed = await this.unauthorizedHandler()
      if (refreshed) return this.uploadForm<T>(path, formData)
    }

    return this.parseResponse<T>(res)
  }
}

export const apiClient = new ApiClient()

export async function fetchPage<T>(path: string): Promise<PageData<T>> {
  return apiClient.get<PageData<T>>(path)
}
