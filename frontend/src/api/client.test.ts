import { describe, expect, it, vi } from 'vitest'
import { ApiError, closurePath, endpointRef, isWaking, placesPath, request, resetWakeUrl } from './client'

const json = (body: unknown, status = 200) =>
  vi.fn<typeof fetch>().mockResolvedValue(new Response(JSON.stringify(body), { status }))

describe('request', () => {
  it('unwraps the data of an ok envelope', async () => {
    const fetchFn = json({ ok: true, data: { status: 'ok' }, meta: { took_ms: 0.1 } })
    await expect(request('/health', {}, fetchFn)).resolves.toEqual({ status: 'ok' })
    expect(fetchFn).toHaveBeenCalledWith('/api/v1/health', expect.objectContaining({ headers: { Accept: 'application/json' } }))
  })

  it('sends JSON bodies with a content type', async () => {
    const fetchFn = json({ ok: true, data: [], meta: {} })
    await request('/journeys', { method: 'POST', body: '{}' }, fetchFn)
    const init = fetchFn.mock.calls[0]?.[1]
    expect(init?.headers).toMatchObject({ 'Content-Type': 'application/json' })
  })

  it('turns an error envelope into an ApiError with code and status', async () => {
    const fetchFn = json({ ok: false, error: { code: 'not_found', message: 'no place or station with id nope' } }, 404)
    const err = await request('/journeys', {}, fetchFn).catch((e: unknown) => e)
    expect(err).toBeInstanceOf(ApiError)
    expect(err).toMatchObject({ code: 'not_found', status: 404, message: 'no place or station with id nope' })
  })

  it('reports a server it cannot reach as a network error', async () => {
    const fetchFn = vi.fn<typeof fetch>().mockRejectedValue(new TypeError('Failed to fetch'))
    await expect(request('/health', {}, fetchFn)).rejects.toMatchObject({ code: 'network' })
  })

  it('reports a non-JSON reply (e.g. a proxy error page) as a bad response', async () => {
    const fetchFn = vi.fn<typeof fetch>().mockResolvedValue(new Response('<html>500</html>', { status: 500 }))
    await expect(request('/health', {}, fetchFn)).rejects.toMatchObject({ code: 'bad_response', status: 500 })
  })

  it('passes aborts through untouched', async () => {
    const abort = new DOMException('aborted', 'AbortError')
    const fetchFn = vi.fn<typeof fetch>().mockRejectedValue(abort)
    await expect(request('/health', {}, fetchFn)).rejects.toBe(abort)
  })
})

describe('paths', () => {
  it('encodes the places query', () => {
    expect(placesPath('  hauz khas & co ', 5)).toBe('/places?q=hauz+khas+%26+co&limit=5')
  })
  it('prefixes endpoint refs with their kind', () => {
    expect(endpointRef({ kind: 'station', id: 'cyber-city' })).toBe('station:cyber-city')
    expect(endpointRef({ kind: 'place', id: 'cyber-city' })).toBe('place:cyber-city')
  })
})

describe('closurePath', () => {
  it('keeps the colons of a closure id and encodes the rest', () => {
    expect(closurePath('segment:yellow:green-park:hauz-khas')).toBe('/disruptions/segment:yellow:green-park:hauz-khas')
    expect(closurePath('station:a b')).toBe('/disruptions/station:a%20b')
  })
})

describe('request while the server is waking up', () => {
  const ok = () => new Response(JSON.stringify({ ok: true, data: { status: 'ok' }, meta: {} }), { status: 200 })
  const asleep = () => new Response('<html>Bad Gateway</html>', { status: 502 })
  const wakeJson = (api: string) => new Response(JSON.stringify({ api }), { status: 200 })

  it('retries a 502 until the server answers, and pings the API address once to wake it', async () => {
    resetWakeUrl()
    const calls: string[] = []
    let apiCalls = 0
    const fetchFn = vi.fn<typeof fetch>(async (input) => {
      const url = String(input)
      calls.push(url)
      if (url === '/wake.json') return wakeJson('https://safar-api.example.com')
      if (url.startsWith('https://')) return new Response(null, { status: 200 })
      apiCalls += 1
      return apiCalls < 3 ? asleep() : ok()
    })
    const pending = request('/health', {}, fetchFn, { pauseMs: 1 })
    await expect(pending).resolves.toEqual({ status: 'ok' })
    expect(apiCalls).toBe(3)
    expect(calls.filter((c) => c === 'https://safar-api.example.com/api/v1/health')).toHaveLength(1)
    const wakeCall = fetchFn.mock.calls.find(([u]) => String(u).startsWith('https://'))
    expect(wakeCall?.[1]).toMatchObject({ mode: 'no-cors' })
    expect(isWaking()).toBe(false)
  })

  it('shows the waking state while it waits', async () => {
    resetWakeUrl()
    let release!: () => void
    const gate = new Promise<void>((r) => (release = r))
    let n = 0
    const fetchFn = vi.fn<typeof fetch>(async (input) => {
      if (String(input) === '/wake.json') return new Response('not found', { status: 404 })
      n += 1
      if (n === 1) return asleep()
      await gate
      return ok()
    })
    const pending = request('/health', {}, fetchFn, { pauseMs: 1 })
    await vi.waitFor(() => expect(isWaking()).toBe(true))
    release()
    await pending
    expect(isWaking()).toBe(false)
  })

  it('gives up with server_asleep after the deadline', async () => {
    resetWakeUrl()
    const fetchFn = vi.fn<typeof fetch>(async (input) => (String(input) === '/wake.json' ? new Response('', { status: 404 }) : asleep()))
    await expect(request('/health', {}, fetchFn, { deadlineMs: 20, pauseMs: 5 })).rejects.toMatchObject({ code: 'server_asleep', status: 502 })
    expect(isWaking()).toBe(false)
  })

  it('does not retry ordinary errors', async () => {
    const fetchFn = json({ ok: false, error: { code: 'invalid_field', message: 'bad' } }, 400)
    await expect(request('/journeys', {}, fetchFn, { pauseMs: 1 })).rejects.toMatchObject({ code: 'invalid_field' })
    expect(fetchFn).toHaveBeenCalledTimes(1)
  })

  it('stops waiting when the request is aborted', async () => {
    resetWakeUrl()
    const ctrl = new AbortController()
    const fetchFn = vi.fn<typeof fetch>(async (input) => (String(input) === '/wake.json' ? new Response('', { status: 404 }) : asleep()))
    const pending = request('/health', { signal: ctrl.signal }, fetchFn, { pauseMs: 50 })
    await vi.waitFor(() => expect(isWaking()).toBe(true))
    ctrl.abort()
    await expect(pending).rejects.toMatchObject({ name: 'AbortError' })
    expect(isWaking()).toBe(false)
  })
})
