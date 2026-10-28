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
  closures: number
  depart: { at: string; hour: number; sunday_fares: boolean }
  fare_note: string
  value_of_time_rs_per_min: number
  stats: { candidates: number; distinct: number; pareto: number; searches: number }
  routes: Route[]
}

// ---- Line closures (GET/POST/DELETE /disruptions) ----

export interface StationRef {
  id: string
  name: string
  lat: number
  lng: number
}

export type Closure =
  | { id: string; type: 'station'; label: string; station: StationRef }
  | {
      id: string
      type: 'segment'
      label: string
      line: { id: string; name: string; color: string }
      from: StationRef
      to: StationRef
    }

export type ClosureRequest =
  | { type: 'station'; station: string }
  | { type: 'segment'; line: string; from: string; to: string }

export interface ClosureList {
  items: Closure[]
  version: number
}

export interface AddClosureResponse extends ClosureList {
  closure: Closure
  added: boolean
}

// ---- Algorithm Lab (POST /lab/compare) ----

export type Algorithm = 'bfs' | 'dfs' | 'ucs' | 'greedy' | 'astar'

export interface LabRequest {
  from: string
  to: string
  algorithms?: Algorithm[]
  include_expansion?: boolean
  heuristic_speed_kmh?: number
  boarding_penalty_min?: number
}

export interface LabItinerary {
  legs: Leg[]
  tickets: Ticket[]
  total_min: number
  fare_rs: number
  distance_km: number
  transfers: number
  walk_min: number
}

export interface LabResult {
  algorithm: Algorithm
  found: boolean
  cost: number
  optimality_gap_pct?: number
  stats: { nodes_expanded: number; nodes_generated: number; max_frontier: number; runtime_us: number }
  itinerary?: LabItinerary
  expansion?: string[]
}

export interface LabResponse {
  from: { id: string; name: string }
  to: { id: string; name: string }
  optimal_cost: number | null
  heuristic_speed_kmh: number
  closures: number
  results: LabResult[]
}

// ---- Trip planner (POST /trips/optimize) ----

export type TripMethod = 'hill_climb' | 'simulated_annealing'

export interface TripRequest {
  stops: string[]
  method?: TripMethod | 'both'
  round_trip?: boolean
  depart_at?: string
  preference?: number
  seed?: number
  restarts?: number
  iterations?: number
}

export interface TourTotals {
  order: number[]
  score: number
  total_min: number
  fare_rs: number
}

export interface CurvePoint {
  evaluations: number
  current: number
  best: number
  temperature?: number
}

export interface TripResult extends TourTotals {
  method: TripMethod
  evaluations: number
  moves: number
  runtime_ms: number
  restarts?: number
  accepted_worse?: number
  gap_pct?: number
  curve: CurvePoint[]
}

export interface TripLeg {
  from: number
  to: number
  route: Route
}

export interface TripResponse {
  stops: Endpoint[]
  round_trip: boolean
  depart: { at: string; hour: number; sunday_fares: boolean }
  value_of_time_rs_per_min: number
  matrix: { score: (number | null)[][]; total_min: (number | null)[][]; fare_rs: (number | null)[][] }
  matrix_ms: number
  input: TourTotals
  optimum: (TourTotals & { evaluations: number; runtime_ms: number }) | null
  results: TripResult[]
  best: TourTotals & { method: TripMethod; legs: TripLeg[] }
  closures: number
  fare_note: string
}
