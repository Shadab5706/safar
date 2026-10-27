// Shapes returned by the Safar C++ backend (backend/src/api). Every response is wrapped as
// { ok: true, data, meta } or { ok: false, error: { code, message } }.

export type Mode = 'walk' | 'metro' | 'cab' | 'auto' | 'bike'
export type RoadMode = 'cab' | 'auto' | 'bike'

export interface ApiMeta {
  request_id?: string
  took_ms?: number
}

export interface ApiErrorBody {
  code: string
  message: string
}

export type Envelope<T> =
  | { ok: true; data: T; meta: ApiMeta }
  | { ok: false; error: ApiErrorBody; meta?: ApiMeta }

export interface Health {
  status: string
  service: string
  version: string
  uptime_s: number
  data: {
    lines: number
    stations: number
    places: number
    segments: number
    road_pairs: number
    graph_nodes: number
    graph_edges: number
  }
}

export interface PlaceResult {
  kind: 'place'
  id: string
  name: string
  lat: number
  lng: number
  category?: string
  city?: string
  nearest_station?: { id: string; distance_km: number }
}

export interface StationResult {
  kind: 'station'
  id: string
  name: string
  lat: number
  lng: number
  lines: string[]
}

export type SearchResult = PlaceResult | StationResult

export interface MetroLine {
  id: string
  name: string
  color: string
  operator: string
  mode: string
  avg_wait_min: number
  stations: string[]
}

export interface MetroStation {
  id: string
  name: string
  lat: number
  lng: number
  lines: string[]
}

export interface Endpoint {
  id: string
  name: string
  type: 'place' | 'station'
  lat: number
  lng: number
}

export type LatLng = [number, number]

export interface DeepLink {
  url: string
  app_url: string | null
  prefilled: boolean
  documented: boolean
}

export interface FareQuote {
  provider: string
  provider_name: string
  product: string
  product_name: string
  mode: RoadMode
  fare_rs: number
  fare_range_rs: [number, number]
  surge: number
  pickup_eta_min: number
  ride_min: number
  total_min: number
  distance_km: number
  source: string
  basis: string
  deep_link: DeepLink | null
}

interface LegBase {
  from: Endpoint
  to: Endpoint
  distance_km: number
  total_min: number
  points: LatLng[]
}

export interface MetroLeg extends LegBase {
  kind: 'metro'
  line: { id: string; name: string; color: string; mode: string }
  stops: number
  ride_min: number
  wait_min: number
  entry_min: number
  exit_min: number
  ticket: number
}

export interface WalkLeg extends LegBase {
  kind: 'walk'
  walk_min: number
}

export interface TransferLeg extends LegBase {
  kind: 'transfer'
  walk_min: number
}

export interface RoadLeg extends LegBase {
  kind: 'road'
  mode: RoadMode
  ride_min: number
  wait_min: number
  fare_rs: number
  fare_source: string
  road_data: string
  options: FareQuote[]
}

export type Leg = MetroLeg | WalkLeg | TransferLeg | RoadLeg

export interface Ticket {
  zone: string
  from: Endpoint
  to: Endpoint
  distance_km: number
  fare_rs: number
  fare_verified: boolean
}

export type RouteTag = 'recommended' | 'fastest' | 'cheapest' | 'fewest_changes' | 'most_expensive'

export interface Route {
  rank: number
  summary: string
  tags: RouteTag[]
  total_min: number
  fare_rs: number
  distance_km: number
  transfers: number
  walk_min: number
  score: number
  found_by: string
  dominated: boolean
  tickets: Ticket[]
  legs: Leg[]
}

export interface JourneyRequest {
  from: string
  to: string
  depart_at?: string
  modes?: Mode[]
  preference?: number
  max_results?: number
}

export interface JourneyResponse {
  from: Endpoint
  to: Endpoint
  depart: { at: string; hour: number; sunday_fares: boolean }
  fare_note: string
  value_of_time_rs_per_min: number
  stats: { candidates: number; distinct: number; pareto: number; searches: number }
  routes: Route[]
}
