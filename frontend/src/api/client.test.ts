import { describe, expect, it, vi } from 'vitest'
import { ApiError, closurePath, endpointRef, placesPath, request } from './client'

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
    const fetchFn = vi.fn<typeof fetch>().mockResolvedValue(new Response('<html>502</html>', { status: 502 }))
    await expect(request('/health', {}, fetchFn)).rejects.toMatchObject({ code: 'bad_response', status: 502 })
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
