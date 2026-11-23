import { useState } from "react"
import type { LockResult } from "../types"
import { USE_MOCK } from "../api"
import Button from "../components/Button"
import Card from "../components/Card"
import Icon from "../components/Icon"
import Stat from "../components/Stat"

function toLocalInputValue(unixSeconds: number) {
  const d = new Date(unixSeconds * 1000)
  const pad = (n: number) => String(n).padStart(2, "0")
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}T${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`
}

export default function LockedScreen({
  result,
  busy,
  onReset,
  onRetry,
  onBack,
  onSchedule,
  onDistribute,
}: {
  result: LockResult | null
  busy: boolean
  onReset: () => void
  onRetry: () => void
  onBack: () => void
  onSchedule?: (unlockTimestamp: number) => Promise<void> | void
  onDistribute: () => void
}) {
  const [localDateTime, setLocalDateTime] = useState<string>(() =>
    toLocalInputValue(result?.unlockTimestamp || Math.floor(Date.now() / 1000) + 600),
  )
  if (!result)
    return (
      <Card className="lock-card" aria-live="polite">
        <div className="icon-tile big">
          {busy ? <span className="spinner" /> : <Icon name="alert" />}
        </div>
        <h2>
          {busy ? "Compressing and encrypting…" : "Paper could not be locked"}
        </h2>
        <p className="muted">
          {busy
            ? "Preparing the secure paper package. Please keep this page open."
            : "Your preview is still available. You can safely try again."}
        </p>
        {!busy && (
          <div className="actions">
            <div className="primary-slot">
              <Button onClick={onRetry}>Try again</Button>
            </div>
            <Button variant="secondary" onClick={onBack}>
              Back to preview
            </Button>
          </div>
        )}
      </Card>
    )
  const saved =
    result.originalBits > 0
      ? Math.round((1 - result.compressedBits / result.originalBits) * 100)
      : 0
  return (
    <Card className="lock-card">
      <div className="icon-tile big">
        <Icon name="lock" />
      </div>
      <span className="eyebrow">
        {USE_MOCK ? "SIMULATED SECURE PACKAGE" : "SECURE PACKAGE READY"}
      </span>
      <h2 className="locked-title">Paper locked</h2>
      <p className="muted">Your paper has completed the security pipeline.</p>
      <div className="stats">
        <Stat
          label="Original size"
          value={`${result.originalBits.toLocaleString()} bits`}
        />
        <span className="stat-arrow">→</span>
        <Stat
          label="Compressed"
          value={`${result.compressedBits.toLocaleString()} bits`}
          note={`${saved}% saved`}
        />
        <span className="stat-arrow">→</span>
        <Stat
          label="Encrypted"
          value={`${result.encryptedBytes.toLocaleString()} bytes`}
        />
      </div>
      <div className="pipeline" aria-label="Security pipeline">
        {["Paper", "Huffman compression", "Encryption", "Saved package"].map(
          (s, i) => (
            <div key={s}>
              <span className="pipeline-dot">{i + 1}</span>
              <span>{s}</span>
            </div>
          ),
        )}
      </div>
      <div
        className={`integrity ${result.verified ? "" : "danger"}`}
        role="status"
      >
        <Icon name={result.verified ? "check" : "alert"} />
        Integrity check {result.verified ? "passed" : "failed"}
      </div>
      <p className="package-copy">
        Saved as <span className="filename">{result.packageFile}</span>. The key
        is stored separately on the server and is never sent to the browser.
      </p>

      <div className="schedule-box">
        <div className="row-between">
          <div>
            <span className="eyebrow">EXAM UNLOCK SCHEDULE (MIN-HEAP TIME-LOCK)</span>
            <p className="small muted">
              {result.unlockTimeFormatted
                ? `Currently scheduled for ${result.unlockTimeFormatted}`
                : "Set when exam centres and student terminals can decrypt this paper."}
            </p>
          </div>
        </div>
        <div className="schedule-controls">
          <input
            type="datetime-local"
            step={1}
            value={localDateTime}
            disabled={busy}
            onChange={(e) => setLocalDateTime(e.target.value)}
          />
          <div className="preset-row">
            {[
              { label: "In 30 secs", secs: 30 },
              { label: "In 1 min", secs: 60 },
              { label: "In 2 mins", secs: 120 },
              { label: "In 10 mins", secs: 600 },
              { label: "In 30 mins", secs: 1800 },
            ].map((p) => (
              <Button
                key={p.secs}
                variant="secondary"
                className="compact-btn"
                disabled={busy}
                onClick={() => {
                  const ts = Math.floor(Date.now() / 1000) + p.secs
                  setLocalDateTime(toLocalInputValue(ts))
                  void onSchedule?.(ts)
                }}
              >
                {p.label}
              </Button>
            ))}
          </div>
        </div>
      </div>

      {USE_MOCK && (
        <p className="mock-note">
          Demo mode: compression, encryption, and saving are simulated. No real
          key or package is created.
        </p>
      )}
      <div className="actions">
        <div className="primary-slot">
          <Button
            disabled={busy}
            onClick={async () => {
              const parsed = Math.floor(new Date(localDateTime).getTime() / 1000)
              if (Number.isFinite(parsed) && parsed > 0 && onSchedule) {
                await onSchedule(parsed)
              }
              onDistribute()
            }}
          >
            Continue to National Broadcast <Icon name="arrow" />
          </Button>
        </div>
        <Button variant="secondary" onClick={onReset}>
          Start over
        </Button>
      </div>
    </Card>
  )
}
