/** Where the phone bottom sheet can rest: a peek at the top of the results, half the screen, or almost all of it. */
export type Snap = 'peek' | 'half' | 'full'

export type SnapOffsets = Record<Snap, number>

/** Distance from the top of the map area to the top of the sheet at each snap, in px. */
export function snapOffsets(height: number, peek = 136): SnapOffsets {
  const full = Math.min(Math.round(height * 0.05), 40)
  const half = Math.round(height * 0.46)
  return { full, half: Math.max(half, full), peek: Math.max(height - peek, half) }
}

/** Keep a dragged sheet between its highest and lowest snap. */
export function clampOffset(offset: number, offsets: SnapOffsets): number {
  return Math.min(Math.max(offset, offsets.full), offsets.peek)
}

const ORDER: readonly Snap[] = ['full', 'half', 'peek'] // top to bottom

/**
 * The snap a released drag settles on. A quick flick (|velocity| px/ms above `flick`) moves to the
 * next snap in its direction; a slow drag lands on the nearest one.
 */
export function settleSnap(offset: number, velocity: number, offsets: SnapOffsets, flick = 0.45): Snap {
  if (velocity > flick) return ORDER.find((s) => offsets[s] > offset + 1) ?? 'peek'
  if (velocity < -flick) return [...ORDER].reverse().find((s) => offsets[s] < offset - 1) ?? 'full'
  let best: Snap = 'half'
  for (const s of ORDER) if (Math.abs(offsets[s] - offset) < Math.abs(offsets[best] - offset)) best = s
  return best
}

/** Where a tap on the handle sends the sheet: up a step, and from the top back to half. */
export function nextSnap(snap: Snap): Snap {
  return snap === 'peek' ? 'half' : snap === 'half' ? 'full' : 'half'
}
