import type { ValidationError } from "../types"
export default function ErrorTable({ errors }: { errors: ValidationError[] }) {
  return (
    <div className="error-table">
      <table>
        <thead>
          <tr>
            <th>Row</th>
            <th>Field</th>
            <th>Problem</th>
          </tr>
        </thead>
        <tbody>
          {errors.map((error, i) => (
            <tr key={i}>
              <td data-label="Row">{error.row === 0 ? "File" : error.row}</td>
              <td data-label="Field">{error.field}</td>
              <td data-label="Problem">{error.problem}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  )
}
