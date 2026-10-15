import Icon from "./Icon"
import type { Screen } from "../types"
import Button from "./Button"
export default function Stepper({
  current,
  busy,
  onBack,
}: {
  current: Screen
  busy: boolean
  onBack: (screen: Screen) => void
}) {
  return (
    <nav aria-label="Paper setting progress" className="stepper">
      <ol>
        {["Upload", "Configure", "Preview", "Lock"].map((label, i) => (
          <li
            key={label}
            className={
              i === current ? "current" : i < current ? "complete" : ""
            }
            aria-current={i === current ? "step" : undefined}
          >
            {i < current ? (
              <Button
                variant="quiet"
                className="step-link"
                disabled={busy}
                onClick={() => onBack(i as Screen)}
                aria-label={`Back to ${label}`}
              >
                <span className="step-number">
                  <Icon name="check" />
                </span>
                <span>{label}</span>
              </Button>
            ) : (
              <span className="step-label">
                <span className="step-number">{i + 1}</span>
                <span>{label}</span>
              </span>
            )}
          </li>
        ))}
      </ol>
    </nav>
  )
}
