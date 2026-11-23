import type { ReactNode } from "react"
export default function Stat({
  label,
  value,
  note,
}: {
  label: string
  value: string
  note?: ReactNode
}) {
  return (
    <div className="stat">
      <p className="eyebrow">{label}</p>
      <p className="stat-value">{value}</p>
      {note && <p className="muted small">{note}</p>}
    </div>
  )
}
