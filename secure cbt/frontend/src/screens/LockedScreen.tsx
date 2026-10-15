import type { LockResult } from "../types"
import { USE_MOCK } from "../api"
import Button from "../components/Button"
import Card from "../components/Card"
import Icon from "../components/Icon"
import Stat from "../components/Stat"
export default function LockedScreen({
  result,
  busy,
  onReset,
  onRetry,
  onBack,
}: {
  result: LockResult | null
  busy: boolean
  onReset: () => void
  onRetry: () => void
  onBack: () => void
}) {
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
      {USE_MOCK && (
        <p className="mock-note">
          Demo mode: compression, encryption, and saving are simulated. No real
          key or package is created.
        </p>
      )}
      <div className="actions">
        <div className="primary-slot">
          <Button onClick={onReset}>
            Start over <Icon name="arrow" />
          </Button>
        </div>
      </div>
    </Card>
  )
}
