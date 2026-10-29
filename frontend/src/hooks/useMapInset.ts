import { createContext, useContext } from 'react'

/** How many px at the bottom of the map the phone sheet covers, so routes are fitted above it. */
export const MapInset = createContext(0)

export const useMapInset = () => useContext(MapInset)
