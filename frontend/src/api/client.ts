import type {
  Envelope,
  Health,
  JourneyRequest,
  JourneyResponse,
  MetroLine,
  MetroStation,
  SearchResult,
} from './types'

export const API_BASE = '/api/v1'

/** An error from the API (its `code`) or from the network (`network`, `bad_response`). */
export class ApiError extends Error {
  readonly code: string
  readonly status: number

  constructor(code: string, message: string, status = 0) {
    super(message)
    this.name = 'ApiError'
    this.code = code
    this.status = status
  }
}

export function isAbortError(err: unknown): boolean {
  return err instanceof DOMException && err.name === 'AbortError'
}

/** Fetch an API path and unwrap the { ok, data } envelope. */
export async function request<T>(path: string, init: RequestInit = {}, fetchFn: typeof fetch = fetch): Promise<T> {
  let res: Response
  try {
    res = await fetchFn(`${API_BASE}${path}`, {
      ...init,
      headers: { Accept: 'application/json', ...(init.body ? { 'Content-Type': 'application/json' } : {}), ...init.headers },
    })
  } catch (err) {
    if (isAbortError(err)) throw err
    throw new ApiError('network', 'Could not reach the Safar server. Check that the backend is running.')
  }

  let body: Envelope<T>
  try {
    body = (await res.json()) as Envelope<T>
  } catch {
    throw new ApiError('bad_response', `The server sent an unreadable response (HTTP ${res.status}).`, res.status)
  }
  if (!body || typeof body !== 'object' || !('ok' in body)) {
    throw new ApiError('bad_response', `The server sent an unexpected response (HTTP ${res.status}).`, res.status)
  }
  if (!body.ok) throw new ApiError(body.error.code, body.error.message, res.status)
  return body.data
}

export function placesPath(query: string, limit = 8): string {
  const params = new URLSearchParams({ q: query.trim(), limit: String(limit) })
  return `/places?${params.toString()}`
}

export function searchPlaces(query: string, signal?: AbortSignal, limit = 8): Promise<SearchResult[]> {
  return request<SearchResult[]>(placesPath(query, limit), { signal })
}

export function planJourney(req: JourneyRequest, signal?: AbortSignal): Promise<JourneyResponse> {
  return request<JourneyResponse>('/journeys', { method: 'POST', body: JSON.stringify(req), signal })
}

export function getMetroLines(signal?: AbortSignal): Promise<MetroLine[]> {
  return request<MetroLine[]>('/metro/lines', { signal })
}

export function getMetroStations(signal?: AbortSignal): Promise<MetroStation[]> {
  return request<MetroStation[]>('/metro/stations', { signal })
}

export function getHealth(signal?: AbortSignal): Promise<Health> {
  return request<Health>('/health', { signal })
}

/** The endpoint string the journeys API accepts: "place:<id>" or "station:<id>". */
export function endpointRef(item: { kind: 'place' | 'station'; id: string }): string {
  return `${item.kind}:${item.id}`
}
