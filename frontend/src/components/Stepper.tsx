import Icon from "./Icon"
import type { Screen } from "../types"
import Button from "./Button"
export default function Stepper({
  current,
  busy,
  canJumpToDistribute = false,
  onBack,
  onJump,
}: {
  current: Screen
  busy: boolean
  canJumpToDistribute?: boolean
  onBack: (screen: Screen) => void
  onJump?: (screen: Screen) => void
}) {
  return (
    <nav aria-label="Secure CBT progress" className="stepper">
      <ol>
        {["Upload", "Configure", "Preview", "Lock & Schedule", "National Broadcast"].map((label, i) => {
          const canClick =
            !busy && (i < current || (canJumpToDistribute && i === 4 && i !== current))
          return (
            <li
              key={label}
              className={
                i === current ? "current" : i < current ? "complete" : ""
              }
              aria-current={i === current ? "step" : undefined}
            >
              {canClick ? (
                <Button
                  variant="quiet"
                  className="step-link"
                  disabled={busy}
                  onClick={() => (i < current ? onBack(i as Screen) : onJump?.(i as Screen))}
                  aria-label={`Go to ${label}`}
                >
                  <span className="step-number">
                    {i < current ? <Icon name="check" /> : i + 1}
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
          )
        })}
      </ol>
    </nav>
  )
}
