import Button from "./Button"
export default function Counter({
  value,
  max,
  label,
  onChange,
}: {
  value: number
  max: number
  label: string
  onChange: (value: number) => void
}) {
  const clamp = (n: number) =>
    onChange(Math.max(0, Math.min(max, Number.isFinite(n) ? Math.trunc(n) : 0)))
  return (
    <div className="counter">
      <Button
        variant="quiet"
        aria-label={`Remove one ${label}`}
        disabled={value === 0}
        onClick={() => clamp(value - 1)}
      >
        −
      </Button>
      <input
        type="number"
        inputMode="numeric"
        min={0}
        max={max}
        step={1}
        value={value}
        aria-label={label}
        onChange={(e) => clamp(Number(e.target.value))}
      />
      <Button
        variant="quiet"
        aria-label={`Add one ${label}`}
        disabled={value === max}
        onClick={() => clamp(value + 1)}
      >
        +
      </Button>
    </div>
  )
}
