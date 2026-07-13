export class ApiError extends Error {
  constructor(
    public code: number,
    message: string,
    public httpStatus?: number,
    public requestId?: string,
  ) {
    super(message)
    this.name = 'ApiError'
  }
}

export interface ApiResponse<T> {
  code: number
  message: string
  data: T
  request_id?: string
}

export interface PageData<T> {
  items: T[]
  next_cursor: string | null
  has_more: boolean
}
