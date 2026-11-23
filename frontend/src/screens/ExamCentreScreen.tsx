import { useEffect, useState } from "react"
import type {
  CentreForwardReceipt,
  NetworkState,
  StudentAttemptStatus,
} from "../types"
import Button from "../components/Button"
import Card from "../components/Card"
import Icon from "../components/Icon"
import Stat from "../components/Stat"

// Default demo student roster registered under the selected exam centre
const DEFAULT_STUDENTS = [
  { roll: "2510013438", name: "Naman" },
  { roll: "2510370185", name: "Aayush Gupta" },
  { roll: "2510012263", name: "Kushagra Bhushan" },
]

// Quick duration presets an invigilator can allocate before or after forwarding
const DURATION_PRESETS = [
  { label: "1 min (Demo)", seconds: 60 },
  { label: "5 mins", seconds: 300 },
  { label: "15 mins", seconds: 900 },
  { label: "30 mins", seconds: 1800 },
  { label: "45 mins", seconds: 2700 },
  { label: "60 mins", seconds: 3600 },
]

// Formats a duration in seconds as MM:SS or HH:MM:SS for display in the table
function formatDurationClock(totalSeconds: number) {
  const s = Math.max(0, totalSeconds)
  const hrs = Math.floor(s / 3600)
  const mins = Math.floor((s % 3600) / 60)
  const secs = s % 60
  if (hrs > 0) {
    return `${String(hrs).padStart(2, "0")}:${String(mins).padStart(2, "0")}:${String(secs).padStart(2, "0")}`
  }
  return `${String(mins).padStart(2, "0")}:${String(secs).padStart(2, "0")}`
}

export default function ExamCentreScreen({
  network,
  receipts,
  busy,
  onForward,
  onAllocateDuration,
  onRefreshNetwork,
  onOpenStudentTerminal,
  onBackToBoard,
}: {
  network: NetworkState | null
  receipts: CentreForwardReceipt[]
  busy: boolean
  onForward: (payload: {
    centreId: string
    studentRolls: string[]
    examDurationSeconds: number
  }) => void
  onAllocateDuration: (centreId: string, examDurationSeconds: number) => void
  onRefreshNetwork: () => void
  onOpenStudentTerminal: (centreId: string, studentRoll: string) => void
  onBackToBoard: () => void
}) {
  // Filter hierarchy nodes down to physical Exam Centres
  const allCentres = (network?.nodes || []).filter((n) => n.role === "Centre")
  const deliveredCentres = allCentres.filter((n) => n.delivered)
  const defaultCentre =
    deliveredCentres.find((c) => c.nodeId === "CTR-UK-01")?.nodeId ||
    deliveredCentres[0]?.nodeId ||
    allCentres.find((c) => c.nodeId === "CTR-UK-01")?.nodeId ||
    "CTR-UK-01"

  const [centreId, setCentreId] = useState(defaultCentre)
  const [students, setStudents] = useState(DEFAULT_STUDENTS)
  const [customRoll, setCustomRoll] = useState("")
  const [durationSeconds, setDurationSeconds] = useState<number>(2700)
  const [customMins, setCustomMins] = useState("45")

  const selectedCentre = allCentres.find((c) => c.nodeId === centreId)
  const isDelivered = Boolean(selectedCentre?.delivered)
  const serverDuration = network?.centreDurations?.[centreId]

  // Keep local duration state synced whenever centre selection or server allocation changes
  useEffect(() => {
    const activeDur = serverDuration ?? 2700
    setDurationSeconds(activeDur)
    setCustomMins(String(Math.max(1, Math.round(activeDur / 60))))
  }, [centreId, serverDuration])

  // Poll the C++ server every 3s so student progress and proctor warnings update live
  useEffect(() => {
    onRefreshNetwork()
    const id = window.setInterval(() => {
      onRefreshNetwork()
    }, 3000)
    return () => window.clearInterval(id)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [])

  // Appends a valid custom student roll number into the local roster queue
  const addCustomStudent = () => {
    const trimmed = customRoll.trim()
    if (!trimmed || students.some((s) => s.roll === trimmed)) return
    setStudents((prev) => [...prev, { roll: trimmed, name: "Candidate" }])
    setCustomRoll("")
  }

  // Applies a chosen duration in seconds both locally and on the C++ backend
  const applyDurationSeconds = (seconds: number) => {
    const clamped = Math.max(30, Math.min(18000, seconds))
    setDurationSeconds(clamped)
    setCustomMins(String(Math.max(1, Math.round(clamped / 60))))
    onAllocateDuration(centreId, clamped)
  }

  // Lookup live attempt status reported by each student terminal at the selected centre
  const centreAttempts = (network?.studentAttempts || []).filter(
    (a) => a.centreId === centreId,
  )
  const getAttemptForRoll = (roll: string): StudentAttemptStatus | undefined =>
    centreAttempts.find((a) => a.studentRoll === roll)

  // Merge forwarded students from local receipts, backend attempts, and LCRS child nodes
  const studentNodesAtCentre = (network?.nodes || []).filter(
    (n) => n.role === "Student" && n.parentId === centreId && n.delivered,
  )
  const centreReceipts = receipts.filter((r) => r.centreId === centreId)
  const monitoredRolls = Array.from(
    new Set([
      ...centreReceipts.map((r) => r.studentRoll),
      ...centreAttempts.map((a) => a.studentRoll),
      ...studentNodesAtCentre.map((n) => n.nodeId.replace(/^STU-/, "")),
    ]),
  )

  // Maps candidate live status string to a clean colour badge class and label
  const renderLiveStatusBadge = (
    roll: string,
    attempt?: StudentAttemptStatus,
  ) => {
    const deliveredNode = studentNodesAtCentre.find(
      (n) => n.nodeId === `STU-${roll}`,
    )
    if (!attempt && !deliveredNode) {
      return <span className="badge neutral">Queued</span>
    }
    if (!attempt || attempt.status === "Forwarded") {
      return <span className="badge ok">Forwarded (Time-Locked)</span>
    }
    if (attempt.status === "Attempting") {
      return <span className="badge warn">Attempting (Fullscreen)</span>
    }
    return (
      <span className="badge ok">
        {attempt.autoSubmitted ? "Auto-Submitted (Timeout)" : "Submitted"}
      </span>
    )
  }

  return (
    <div className="phase-stack">
      <Card>
        <div className="card-heading row-between">
          <div>
            <span className="eyebrow">EXAM CENTRE CONSOLE</span>
            <h2>Receive, Time-Allocate &amp; Forward Paper</h2>
            <p className="muted">
              Select your Exam Centre to inspect its L4 encrypted package, allocate exam duration, forward copies to student terminals, and monitor live proctoring alerts.
            </p>
          </div>
          <Button
            variant="secondary"
            className="compact-btn"
            disabled={busy}
            onClick={onBackToBoard}
          >
            Back to Board
          </Button>
        </div>

        <label className="field">
          <span className="eyebrow">SELECT EXAM CENTRE</span>
          <select
            value={centreId}
            disabled={busy}
            onChange={(e) => setCentreId(e.target.value)}
          >
            {(deliveredCentres.length > 0 ? deliveredCentres : allCentres).map(
              (c) => (
                <option key={c.nodeId} value={c.nodeId}>
                  {c.nodeId} — {c.name}{" "}
                  {c.delivered ? "(Delivered)" : "(Pending)"}
                </option>
              ),
            )}
          </select>
        </label>

        <div className="stats compact-stats" style={{ marginTop: 20 }}>
          <Stat
            label="L4 Package Status"
            value={isDelivered ? "Received" : "Not Delivered"}
            note={
              isDelivered && selectedCentre
                ? `${selectedCentre.payloadBytes.toLocaleString()} bytes`
                : "Broadcast from Central Board first"
            }
          />
          <span className="stat-arrow">·</span>
          <Stat
            label="Allocated Exam Time"
            value={`${Math.max(1, Math.round(durationSeconds / 60))} mins`}
            note={`${durationSeconds.toLocaleString()}s fullscreen timer`}
          />
          <span className="stat-arrow">·</span>
          <Stat
            label="Scheduled Unlock"
            value={
              network?.unlockTimestamp
                ? new Date(network.unlockTimestamp * 1000).toLocaleTimeString()
                : network?.unlockTimeFormatted || "Not set"
            }
            note="Min-Heap Time-Lock"
          />
        </div>

        <div className="schedule-box">
          <span className="eyebrow">CENTRE EXAM DURATION ALLOCATION</span>
          <p className="muted small" style={{ marginTop: 6 }}>
            Choose how long candidates at {centreId} have inside Fullscreen Mode once the paper unlocks. The timer auto-submits at 00:00.
          </p>
          <div className="preset-row" style={{ marginTop: 12 }}>
            {DURATION_PRESETS.map((preset) => (
              <Button
                key={preset.seconds}
                variant={
                  durationSeconds === preset.seconds ? "primary" : "secondary"
                }
                className="compact-btn"
                disabled={busy}
                onClick={() => applyDurationSeconds(preset.seconds)}
              >
                {preset.label}
              </Button>
            ))}
          </div>
          <div className="schedule-controls">
            <label className="field" style={{ minWidth: 160 }}>
              <span className="eyebrow">CUSTOM MINUTES</span>
              <input
                type="number"
                min={1}
                max={300}
                value={customMins}
                disabled={busy}
                onChange={(e) => setCustomMins(e.target.value)}
              />
            </label>
            <Button
              variant="secondary"
              className="compact-btn"
              disabled={busy || !customMins.trim()}
              onClick={() => {
                const parsed = Number(customMins)
                if (Number.isFinite(parsed) && parsed >= 1) {
                  applyDurationSeconds(Math.round(parsed) * 60)
                }
              }}
            >
              Apply Duration
            </Button>
          </div>
        </div>
      </Card>

      <Card>
        <div className="card-heading">
          <h2>Queued Student Roll Numbers</h2>
          <p className="muted">
            Each student receives an L5 time-locked package synced with the {Math.max(1, Math.round(durationSeconds / 60))}-minute centre timer.
          </p>
        </div>

        <div className="student-queue">
          {students.map((s) => {
            const attempt = getAttemptForRoll(s.roll)
            return (
              <div key={s.roll} className="student-row">
                <div>
                  <span className="mono strong">{s.roll}</span>
                  <span className="muted" style={{ marginLeft: 10 }}>
                    {s.name}
                  </span>
                </div>
                <div className="header-right">
                  {renderLiveStatusBadge(s.roll, attempt)}
                  {attempt && attempt.exitWarnings > 0 && (
                    <span className="badge danger">
                      FS Exits: {attempt.exitWarnings}
                    </span>
                  )}
                </div>
              </div>
            )
          })}
        </div>

        <div className="inline-form" style={{ marginTop: 16 }}>
          <input
            type="text"
            placeholder="Add custom roll no. (e.g. 2510019999)"
            value={customRoll}
            disabled={busy}
            onChange={(e) => setCustomRoll(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === "Enter") {
                e.preventDefault()
                addCustomStudent()
              }
            }}
          />
          <Button
            variant="secondary"
            className="compact-btn"
            disabled={busy || !customRoll.trim()}
            onClick={addCustomStudent}
          >
            Add Roll No.
          </Button>
        </div>

        <div className="actions">
          <div className="primary-slot">
            <Button
              disabled={busy || !isDelivered || students.length === 0}
              onClick={() =>
                onForward({
                  centreId,
                  studentRolls: students.map((s) => s.roll),
                  examDurationSeconds: durationSeconds,
                })
              }
            >
              {busy
                ? "Forwarding…"
                : "Forward Time-Locked Paper to Student Terminals"}
              <Icon name="arrow" />
            </Button>
          </div>
        </div>
      </Card>

      {monitoredRolls.length > 0 && (
        <Card>
          <div className="card-heading row-between">
            <div>
              <span className="eyebrow">LIVE PROCTOR &amp; SUBMISSION SYNC</span>
              <h2>Candidate Terminal Monitor ({centreId})</h2>
              <p className="muted">
                Auto-refreshes every 3 seconds to track forwarded papers, live MCQ progress, fullscreen exit warnings, and final submissions.
              </p>
            </div>
            <div className="header-right">
              <Button
                variant="secondary"
                className="compact-btn"
                disabled={busy}
                onClick={onRefreshNetwork}
              >
                Refresh Status
              </Button>
              <Button
                onClick={() =>
                  onOpenStudentTerminal(centreId, monitoredRolls[0])
                }
              >
                Open Student Terminal <Icon name="arrow" />
              </Button>
            </div>
          </div>

          <div className="data-table-wrap">
            <table className="clean-table">
              <thead>
                <tr>
                  <th>Student Terminal</th>
                  <th>Live Status</th>
                  <th>Progress</th>
                  <th>Proctor Alerts</th>
                  <th>Timer Left</th>
                  <th>Action</th>
                </tr>
              </thead>
              <tbody>
                {monitoredRolls.map((roll) => {
                  const receipt = centreReceipts.find(
                    (r) => r.studentRoll === roll,
                  )
                  const attempt = getAttemptForRoll(roll)
                  const node = studentNodesAtCentre.find(
                    (n) => n.nodeId === `STU-${roll}`,
                  )
                  const payloadBytes =
                    receipt?.payloadBytes ?? node?.payloadBytes ?? 0
                  const keyFp =
                    receipt?.keyFingerprint || node?.keyFingerprint || "—"
                  const warns = attempt?.exitWarnings ?? 0

                  return (
                    <tr key={roll}>
                      <td>
                        <div className="mono strong">STU-{roll}</div>
                        <div className="muted small">
                          {payloadBytes > 0
                            ? `${payloadBytes.toLocaleString()} B · Key ${keyFp}`
                            : `Roll ${roll}`}
                        </div>
                      </td>
                      <td>
                        {renderLiveStatusBadge(roll, attempt)}
                        {attempt?.updatedAt && (
                          <div className="muted small" style={{ marginTop: 4 }}>
                            {attempt.updatedAt}
                          </div>
                        )}
                      </td>
                      <td className="mono">
                        {attempt && attempt.totalQuestions > 0
                          ? `Answered ${attempt.answeredCount}/${attempt.totalQuestions} · Review ${attempt.reviewCount}`
                          : "Awaiting Start"}
                      </td>
                      <td>
                        <span className={warns > 0 ? "badge danger" : "badge ok"}>
                          {warns > 0 ? `FS Exit Warnings: ${warns}` : "0 Alerts"}
                        </span>
                      </td>
                      <td className="mono">
                        {attempt
                          ? formatDurationClock(attempt.timeRemainingSeconds)
                          : formatDurationClock(durationSeconds)}
                      </td>
                      <td>
                        <Button
                          variant="secondary"
                          className="compact-btn"
                          onClick={() => onOpenStudentTerminal(centreId, roll)}
                        >
                          Open Terminal
                        </Button>
                      </td>
                    </tr>
                  )
                })}
              </tbody>
            </table>
          </div>
        </Card>
      )}
    </div>
  )
}
