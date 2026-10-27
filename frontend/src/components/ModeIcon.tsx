import { Bike, Car, CarTaxiFront, Footprints, TrainFront, type LucideProps } from 'lucide-react'
import type { Mode } from '../api/types'

const ICONS = { walk: Footprints, metro: TrainFront, cab: Car, auto: CarTaxiFront, bike: Bike } as const

export function ModeIcon({ mode, ...props }: { mode: Mode } & LucideProps) {
  const Icon = ICONS[mode]
  return <Icon aria-hidden="true" {...props} />
}
