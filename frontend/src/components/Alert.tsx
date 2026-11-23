import { ApiError } from "../api"
import Icon from "./Icon"
export default function Alert({ error }: { error: Error | null }) {
  if (!error) return null
  return (
    <div role="alert" className="alert danger">
      <Icon name="alert" />
      <div>
        <strong>{error.message}</strong>
        {error instanceof ApiError && error.problems.length > 0 && (
          <ul>
            {error.problems.map((p, i) => (
              <li key={i}>{p}</li>
            ))}
          </ul>
        )}
      </div>
    </div>
  )
}
