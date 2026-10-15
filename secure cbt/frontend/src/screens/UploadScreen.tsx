import { useRef } from "react"
import type { UploadResult } from "../types"
import { mockFile, USE_MOCK } from "../api"
import Button from "../components/Button"
import Card from "../components/Card"
import DropZone from "../components/DropZone"
import ErrorTable from "../components/ErrorTable"
import Icon from "../components/Icon"
export default function UploadScreen({
  file,
  result,
  busy,
  integrityFailure,
  onIntegrityFailure,
  onFile,
  onUpload,
  onContinue,
}: {
  file: File | null
  result: UploadResult | null
  busy: boolean
  integrityFailure: boolean
  onIntegrityFailure: (v: boolean) => void
  onFile: (f: File) => void
  onUpload: () => void
  onContinue: () => void
}) {
  const input = useRef<HTMLInputElement>(null)
  const invalid = result && !result.isValid
  const empty = result?.isValid && result.isEmptyBank
  const success = result?.isValid && !result.isEmptyBank && result.canContinue
  return (
    <>
      <Card>
        <div className="card-heading">
          <h2>Import questions</h2>
          <p className="muted">
            Start with a single CSV containing your exam questions.
          </p>
        </div>
        {!file ? (
          <DropZone disabled={busy} onFile={onFile} />
        ) : (
          <div
            className={`upload-state ${invalid ? "invalid" : ""}`}
            aria-live="polite"
            aria-busy={busy}
          >
            <div className={`icon-tile ${empty ? "amber" : ""}`}>
              {busy ? (
                <span className="spinner" />
              ) : (
                <Icon
                  name={invalid || empty ? "alert" : success ? "check" : "file"}
                />
              )}
            </div>
            <h3>
              {busy ? (
                "Validating your question bank…"
              ) : invalid ? (
                `We found ${result.errors.length} problem${
                  result.errors.length === 1 ? "" : "s"
                } in ${result.filename}`
              ) : empty ? (
                "No questions found"
              ) : success ? (
                `${result.validCount} questions validated`
              ) : (
                <span className="filename">{file.name}</span>
              )}
            </h3>
            <p className="muted filename">
              {empty
                ? "No questions found — the file has the right header but no question rows."
                : success
                  ? `${result.filename} · ${result.validCount} rows`
                  : `${file.name} · ${
                      file.size < 1024
                        ? `${file.size} bytes`
                        : `${(file.size / 1024).toFixed(1)} KB`
                    }`}
            </p>
            {invalid && <ErrorTable errors={result.errors} />}
            {empty && (
              <a
                className="template-link"
                href="/sample-questions.csv"
                download
              >
                Download the template
              </a>
            )}
            {success &&
              result.warnings?.map((w, i) => (
                <div className="alert warning" key={i}>
                  <Icon name="alert" />
                  <span>{w}</span>
                </div>
              ))}
          </div>
        )}
        {!file && (
          <p className="template-help">
            Not sure how to format your file?{" "}
            <a href="/sample-questions.csv" download>
              Download the template
            </a>
          </p>
        )}
        <input
          ref={input}
          type="file"
          accept=".csv,text/csv"
          className="sr-only"
          disabled={busy}
          onChange={(e) => {
            if (e.target.files?.[0]) onFile(e.target.files[0])
            e.target.value = ""
          }}
        />
        <div className="actions">
          <div className="primary-slot">
            {!invalid && !empty && (
              <Button
                disabled={busy || !file}
                onClick={success ? onContinue : onUpload}
              >
                {busy
                  ? "Validating…"
                  : success
                    ? "Continue"
                    : "Upload & Validate"}
                {!busy && <Icon name="arrow" />}
              </Button>
            )}
          </div>
          <Button
            variant="secondary"
            disabled={busy}
            onClick={() => input.current?.click()}
          >
            {file ? "Upload a different file" : "Choose a CSV file"}
          </Button>
        </div>
        <div className="card-footnote">
          <Icon
            name={success ? "check" : "alert"}
            className={success ? "" : "amber"}
          />
          <span>
            {success
              ? "Question bank validated and ready to configure."
              : "Your CSV is validated before any questions are selected."}
          </span>
        </div>
      </Card>
      {USE_MOCK && (
        <details className="mock-tools">
          <summary>
            <span className="mock-badge">DEMO MODE</span> Try a sample scenario
          </summary>
          <p>
            Uploads are validated locally. Locking statistics are simulated; no
            package is saved.
          </p>
          <div className="mock-buttons">
            {(["bank", "errors", "empty", "server"] as const).map((s, i) => (
              <Button
                key={s}
                variant="secondary"
                disabled={busy}
                onClick={() => onFile(mockFile(s))}
              >
                {
                  [
                    "72-question bank",
                    "Validation errors",
                    "Empty bank",
                    "Server error",
                  ][i]
                }
              </Button>
            ))}
          </div>
          <label className="checkbox-label">
            <input
              type="checkbox"
              checked={integrityFailure}
              disabled={busy}
              onChange={(e) => onIntegrityFailure(e.target.checked)}
            />{" "}
            Simulate failed integrity check when locking
          </label>
          <p className="small">
            Choose a scenario, then select Upload & Validate. The downloadable
            template also works with real file uploads.
          </p>
        </details>
      )}
    </>
  )
}
