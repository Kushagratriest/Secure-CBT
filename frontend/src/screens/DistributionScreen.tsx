import { useState } from "react"
import type { DistNode, LeakTraceReport, NetworkState } from "../types"
import Button from "../components/Button"
import Card from "../components/Card"
import Icon from "../components/Icon"
import Stat from "../components/Stat"

type StateFilter = "with-centres" | "all-36"

export default function DistributionScreen({
  network,
  leakReport,
  busy,
  onRefresh,
  onSchedule,
  onBroadcast,
  onAuditAction,
  onTraceLeak,
  onContinuePhase3,
  onBack,
}: {
  network: NetworkState | null
  leakReport: LeakTraceReport | null
  busy: boolean
  onRefresh: () => void
  onSchedule: (payload: { unlockTimestamp?: number; minutesFromNow?: number }) => void
  onBroadcast: () => void
  onAuditAction: (action: "verify" | "tamper" | "restore", index?: number) => void
  onTraceLeak: (nodeId: string) => void
  onContinuePhase3: () => void
  onBack: () => void
}) {
  const [stateFilter, setStateFilter] = useState<StateFilter>("with-centres")
  const [search, setSearch] = useState("")
  const [leakNodeId, setLeakNodeId] = useState("CTR-UK-01")

  if (!network) {
    return (
      <Card className="lock-card">
        <div className="icon-tile big">
          <span className="spinner" />
        </div>
        <h2>Loading distribution hierarchy…</h2>
        <p className="muted">Fetching AVL Registry & LCRS Tree from the C++ server.</p>
      </Card>
    )
  }

  const states = network.nodes.filter((n) => n.role === "State")
  const centres = network.nodes.filter((n) => n.role === "Centre")
  const deliveredCentres = centres.filter((c) => c.delivered)
  const deliveredCount = network.hops.filter((h) => h.status === "Delivered").length

  const hasDescendantCentre = (stateId: string) => {
    const children = network.nodes.filter((n) => n.parentId === stateId)
    for (const child of children) {
      if (child.role === "Centre") return true
      const grand = network.nodes.filter((n) => n.parentId === child.nodeId)
      for (const g of grand) {
        if (g.role === "Centre") return true
        if (network.nodes.some((n) => n.parentId === g.nodeId && n.role === "Centre")) {
          return true
        }
      }
    }
    return false
  }

  const query = search.trim().toLowerCase()
  const visibleStates = states.filter((s) => {
    if (stateFilter === "with-centres" && !hasDescendantCentre(s.nodeId)) return false
    if (query && !s.name.toLowerCase().includes(query) && !s.nodeId.toLowerCase().includes(query)) {
      return false
    }
    return true
  })

  const statusBadge = (status: DistNode["status"]) => {
    if (status === "Registered") return "badge ok"
    if (status === "Pending") return "badge warn"
    return "badge danger"
  }

  return (
    <div className="phase-stack">
      <Card>
        <div className="card-heading row-between">
          <div>
            <span className="eyebrow">MIN-HEAP EXAM SCHEDULE</span>
            <h2>
              {network.unlockTimestamp
                ? `Unlock Scheduled: ${new Date(network.unlockTimestamp * 1000).toLocaleString()}`
                : network.unlockTimeFormatted
                  ? `Unlock Scheduled: ${network.unlockTimeFormatted}`
                  : "Exam Unlock Time Not Set"}
            </h2>
            <p className="muted">
              All 36 Indian States/UTs and exam centres stay cryptographically time-locked until this timestamp.
            </p>
          </div>
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
                onClick={() =>
                  onSchedule({ unlockTimestamp: Math.floor(Date.now() / 1000) + p.secs })
                }
              >
                {p.label}
              </Button>
            ))}
          </div>
        </div>

        <div className="stats compact-stats">
          <Stat
            label="AVL Registry"
            value={`${network.totalNodes} nodes`}
            note={`Height ${network.avlHeight} · ${network.isBalanced ? "Balanced" : "Unbalanced"}`}
          />
          <span className="stat-arrow">·</span>
          <Stat
            label="States & UTs"
            value={`${states.length} / 36`}
            note={`${centres.length} Exam Centres`}
          />
          <span className="stat-arrow">·</span>
          <Stat
            label="Onion Broadcast"
            value={network.broadcastDone ? `${deliveredCount} hops` : "Ready"}
            note={
              network.hasPackage
                ? `${network.packageEncryptedBytes.toLocaleString()} B L0 package`
                : "Lock paper first"
            }
          />
        </div>

        <div className="broadcast-cta-row">
          <Button
            disabled={busy || !network.hasPackage}
            onClick={onBroadcast}
          >
            {busy
              ? "Broadcasting…"
              : network.broadcastDone
                ? "Re-Broadcast Paper Across All 36 Indian States & Centres"
                : "Broadcast Paper Across All 36 Indian States & Centres"}
          </Button>
          {network.broadcastDone && (
            <Button variant="secondary" disabled={busy} onClick={onContinuePhase3}>
              Open Exam Centre Console <Icon name="arrow" />
            </Button>
          )}
        </div>
      </Card>

      <Card>
        <div className="card-heading row-between">
          <div>
            <h2>All 36 Indian States & Union Territories</h2>
            <p className="muted">
              Showing {visibleStates.length} of {states.length} States/UTs in the L1 National Distribution Tree.
            </p>
          </div>
          <div className="header-right">
            <Button variant="secondary" className="compact-btn" disabled={busy} onClick={onRefresh}>
              Refresh
            </Button>
          </div>
        </div>

        <div className="filter-bar">
          <div className="seg-tabs compact-seg" role="tablist" aria-label="State filter">
            <button
              type="button"
              role="tab"
              aria-selected={stateFilter === "with-centres"}
              className={`seg-tab ${stateFilter === "with-centres" ? "active" : ""}`}
              onClick={() => setStateFilter("with-centres")}
            >
              States with Centres
            </button>
            <button
              type="button"
              role="tab"
              aria-selected={stateFilter === "all-36"}
              className={`seg-tab ${stateFilter === "all-36" ? "active" : ""}`}
              onClick={() => setStateFilter("all-36")}
            >
              All 36 States & UTs
            </button>
          </div>
          <input
            type="search"
            className="search-input"
            placeholder="Search state or UT (e.g. Uttarakhand, Delhi)…"
            value={search}
            onChange={(e) => setSearch(e.target.value)}
          />
        </div>

        <div className="state-grid">
          {visibleStates.map((st) => (
            <div key={st.nodeId} className={`state-tile ${st.delivered ? "delivered" : ""}`}>
              <div className="row-between">
                <span className="mono small muted">{st.nodeId}</span>
                <span className={statusBadge(st.status)}>{st.status}</span>
              </div>
              <div className="state-name">{st.name}</div>
              <div className="small muted">
                {st.delivered
                  ? `Delivered · ${st.payloadBytes.toLocaleString()} B`
                  : "Awaiting broadcast"}
              </div>
            </div>
          ))}
        </div>
      </Card>

      <details className="mock-tools">
        <summary>Invigilator Verification: SHA-256 Audit & Leak Traceability</summary>
        <div className="forensics-grid">
          <div className="forensics-panel">
            <div className="row-between">
              <div>
                <h3>SHA-256 Audit Chain</h3>
                <p className="small muted">
                  {network.audit.entries.length} immutable custody records ·{" "}
                  {network.audit.intact ? "Chain Intact" : `Tampered at #${network.audit.brokenAt}`}
                </p>
              </div>
              <div className="preset-row">
                <Button
                  variant="secondary"
                  className="compact-btn"
                  disabled={busy}
                  onClick={() => onAuditAction("verify")}
                >
                  Verify
                </Button>
                <Button
                  variant="secondary"
                  className="compact-btn"
                  disabled={busy || network.audit.entries.length === 0}
                  onClick={() => onAuditAction("tamper", 1)}
                >
                  Simulate Tamper
                </Button>
                {!network.audit.intact && (
                  <Button
                    variant="secondary"
                    className="compact-btn"
                    disabled={busy}
                    onClick={() => onAuditAction("restore")}
                  >
                    Restore
                  </Button>
                )}
              </div>
            </div>

            {/* Plain-English explanation callout shown whenever tamper verification fails */}
            {!network.audit.intact && (
              <div className="alert danger" style={{ marginTop: 14, marginBottom: 0 }}>
                <Icon name="alert" />
                <span>
                  Tamper Detected at Hop #{network.audit.brokenAt}: The recipient node was altered to &quot;ROGUE-NODE&quot;. Recomputing SHA-256(index | from | to | timestamp | prevHash) no longer matches the entry&apos;s sealed hash.
                </span>
              </div>
            )}

            {/* Compact preview table showing the first 5 SHA-256 linked audit hops (and the tampered hop if >= 5) */}
            {network.audit.entries.length > 0 && (() => {
              const previewEntries = network.audit.entries.filter(
                (e, i) => i < 5 || (!network.audit.intact && e.index === network.audit.brokenAt),
              )
              return (
                <div className="data-table-wrap">
                  <table className="clean-table">
                    <thead>
                      <tr>
                        <th>Hop #</th>
                        <th>Transfer (From → To)</th>
                        <th>Prev Hash</th>
                        <th>Sealed Hash</th>
                        <th>Integrity</th>
                      </tr>
                    </thead>
                    <tbody>
                      {previewEntries.map((entry) => {
                        const isBroken =
                          !network.audit.intact && entry.index === network.audit.brokenAt
                        return (
                          <tr key={entry.index} className={isBroken ? "tampered-row" : ""}>
                            <td className="mono">#{entry.index}</td>
                            <td className="mono">
                              {entry.fromNode} → {entry.toNode}
                            </td>
                            <td className="mono muted">{entry.prevHash}</td>
                            <td className="mono">{entry.hash}</td>
                            <td>
                              {isBroken ? (
                                <span className="badge danger">Hash Mismatch</span>
                              ) : (
                                <span className="badge ok">Verified</span>
                              )}
                            </td>
                          </tr>
                        )
                      })}
                    </tbody>
                  </table>
                </div>
              )
            })()}
          </div>

          <div className="forensics-panel">
            <h3>AVL Leak-Source Forensics</h3>
            <p className="small muted">
              Trace any leaked encrypted/decrypted paper back to its exact node using AVL key fingerprints.
            </p>
            <div className="inline-form">
              <select
                value={leakNodeId}
                disabled={busy}
                onChange={(e) => setLeakNodeId(e.target.value)}
              >
                {(deliveredCentres.length > 0 ? deliveredCentres : centres).map((c) => (
                  <option key={c.nodeId} value={c.nodeId}>
                    {c.nodeId} — {c.name}
                  </option>
                ))}
              </select>
              <Button
                variant="secondary"
                className="compact-btn"
                disabled={busy || !network.broadcastDone}
                onClick={() => onTraceLeak(leakNodeId)}
              >
                Trace Leak
              </Button>
            </div>
            {leakReport && (
              <div className="integrity" role="status">
                <Icon name="check" />
                <span>
                  Traced to <strong>{leakReport.culpritNodeId}</strong> ({leakReport.culpritName}) ·{" "}
                  {leakReport.chainOfCustody.join(" → ")}
                </span>
              </div>
            )}
          </div>
        </div>
      </details>

      <div className="actions">
        <div className="primary-slot">
          <Button disabled={busy || !network.broadcastDone} onClick={onContinuePhase3}>
            Open Exam Centre Console <Icon name="arrow" />
          </Button>
        </div>
        <Button variant="secondary" disabled={busy} onClick={onBack}>
          Back
        </Button>
      </div>
    </div>
  )
}
