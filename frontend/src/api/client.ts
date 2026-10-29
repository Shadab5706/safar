import type {
  AddClosureResponse,
  ClosureList,
  ClosureRequest,
  Envelope,
  Health,
  JourneyRequest,
  JourneyResponse,
  LabRequest,
  LabResponse,
  MetroLine,
  MetroStation,
  SearchResult,
  TripRequest,
  TripResponse,
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

// ---- Waking a sleeping server ----
// Free hosting puts the API to sleep after ~15 idle minutes. While it starts up, the
// site's proxy answers 502/503/504, and requests through the proxy do not wake it.
// So the browser pings the API's own address (from /wake.json, served by nginx) and
// retries until it answers, showing a "waking up" banner meanwhile.

const WAKE_STATUSES = new Set([502, 503, 504])
const WAKE_DEADLINE_MS = 120_000

let wakingCount = 0
const wakingListeners = new Set<() => void>()

/** True while at least one request is waiting for the server to wake up. */
export function isWaking(): boolean {
  return wakingCount > 0
}

/** Subscribe to isWaking() changes (shaped for useSyncExternalStore). */
export function subscribeWaking(listener: () => void): () => void {
  wakingListeners.add(listener)
  return () => wakingListeners.delete(listener)
}

function changeWaking(delta: number) {
  wakingCount += delta
  wakingListeners.forEach((l) => l())
}

let wakeUrl: Promise<string | null> | null = null

/** For tests: forget the cached /wake.json answer. */
export function resetWakeUrl() {
  wakeUrl = null
}

function apiWakeUrl(fetchFn: typeof fetch): Promise<string | null> {
  wakeUrl ??= fetchFn('/wake.json', { cache: 'no-store' })
    .then((r) => (r.ok ? (r.json() as Promise<{ api?: unknown }>) : null))
    .then((j) => (typeof j?.api === 'string' && j.api.startsWith('https://') ? `${j.api.replace(/\/$/, '')}${API_BASE}/health` : null))
    .catch(() => null)
  return wakeUrl
}

/** Fire-and-forget ping to the API's own address, which starts a sleeping server. No-op locally. */
export async function wakeApi(fetchFn: typeof fetch = fetch): Promise<void> {
  const url = await apiWakeUrl(fetchFn)
  if (url) fetchFn(url, { mode: 'no-cors', cache: 'no-store' }).catch(() => {})
}

function pause(ms: number, signal?: AbortSignal | null): Promise<void> {
  return new Promise((resolve, reject) => {
    if (signal?.aborted) return reject(new DOMException('Aborted', 'AbortError'))
    const t = setTimeout(resolve, ms)
    signal?.addEventListener(
      'abort',
      () => {
        clearTimeout(t)
        reject(new DOMException('Aborted', 'AbortError'))
      },
      { once: true },
    )
  })
}

export interface RetryOptions {
  deadlineMs?: number // how long to wait for a sleeping server before giving up
  pauseMs?: number // first pause between attempts (doubles once, then stays)
}

/** Fetch an API path and unwrap the { ok, data } envelope. Waits for a sleeping server. */
export async function request<T>(
  path: string,
  init: RequestInit = {},
  fetchFn: typeof fetch = fetch,
  { deadlineMs = WAKE_DEADLINE_MS, pauseMs = 2000 }: RetryOptions = {},
): Promise<T> {
  const deadline = Date.now() + deadlineMs
  let attempt = 0
  let res: Response
  try {
    for (;;) {
      try {
        res = await fetchFn(`${API_BASE}${path}`, {
          ...init,
          headers: { Accept: 'application/json', ...(init.body ? { 'Content-Type': 'application/json' } : {}), ...init.headers },
        })
      } catch (err) {
        if (isAbortError(err)) throw err
        throw new ApiError('network', 'Could not reach the Safar server. Check that the backend is running.')
      }
      if (!WAKE_STATUSES.has(res.status)) break
      if (Date.now() >= deadline) {
        throw new ApiError('server_asleep', 'The server is still starting up. Please try again in a minute.', res.status)
      }
      if (attempt === 0) {
        changeWaking(+1)
        void wakeApi(fetchFn)
      }
      attempt += 1
      await pause(attempt < 3 ? pauseMs : pauseMs * 2, init.signal)
    }
  } finally {
    if (attempt > 0) changeWaking(-1)
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

export function getClosures(signal?: AbortSignal): Promise<ClosureList> {
  return request<ClosureList>('/disruptions', { signal })
}

export function addClosure(req: ClosureRequest): Promise<AddClosureResponse> {
  return request<AddClosureResponse>('/disruptions', { method: 'POST', body: JSON.stringify(req) })
}

/** DELETE path for one closure; ids look like "station:<id>" or "segment:<line>:<a>:<b>". */
export function closurePath(id: string): string {
  return `/disruptions/${id.split(':').map(encodeURIComponent).join(':')}`
}

export function removeClosure(id: string): Promise<ClosureList> {
  return request<ClosureList>(closurePath(id), { method: 'DELETE' })
}

export function clearClosures(): Promise<ClosureList> {
  return request<ClosureList>('/disruptions', { method: 'DELETE' })
}

export function compareAlgorithms(req: LabRequest, signal?: AbortSignal): Promise<LabResponse> {
  return request<LabResponse>('/lab/compare', { method: 'POST', body: JSON.stringify(req), signal })
}

export function optimizeTrip(req: TripRequest, signal?: AbortSignal): Promise<TripResponse> {
  return request<TripResponse>('/trips/optimize', { method: 'POST', body: JSON.stringify(req), signal })
}

/** The endpoint string the journeys API accepts: "place:<id>" or "station:<id>". */
export function endpointRef(item: { kind: 'place' | 'station'; id: string }): string {
  return `${item.kind}:${item.id}`
}
