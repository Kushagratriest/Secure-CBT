import { useEffect, useRef, useState } from "react"
import { queryStudentPortal, reportStudentStatus } from "../api"
import type { NetworkState, StudentPortalResponse } from "../types"
import Button from "../components/Button"
import Card from "../components/Card"
import Icon from "../components/Icon"
import Stat from "../components/Stat"

interface McqOption {
  label: string
  text: string
}

interface ParsedQuestion {
  number: number
  id: string
  subject: string
  topic: string
  marks: string
  stem: string
  options: McqOption[]
}

function parseMcqQuestion(block: string, index: number): ParsedQuestion {
  const lines = block.split("\n")
  const header = lines[0] || ""
  const rawBody = lines.slice(1).join(" ").replace(/\s+/g, " ").trim()
  const match = header.match(/^\[([^\]]+)\]\s*\(([^/]+)\s*\/\s*([^,]+),\s*(\d+)\s*marks\)$/i)

  const id = match?.[1] || `Q${index + 1}`
  const subject = match?.[2]?.trim() || "Exam"
  const topic = match?.[3]?.trim() || "General"
  const marks = match?.[4] || ""
  const body = rawBody || header

  // Split "(A) ... (B) ... (C) ... (D) ... (E) ..." into interactive MCQ options
  const firstOptIdx = body.search(/\([A-E]\)\s+/)
  let stem = body
  const options: McqOption[] = []

  if (firstOptIdx !== -1) {
    stem = body.slice(0, firstOptIdx).trim()
    const optChunk = body.slice(firstOptIdx)
    const tokenRegex = /\(([A-E])\)\s+([\s\S]*?)(?=\s*\([A-E]\)\s+|$)/g
    let m: RegExpExecArray | null
    while ((m = tokenRegex.exec(optChunk)) !== null) {
      options.push({ label: m[1], text: m[2].trim() })
    }
  }

  return { number: index + 1, id, subject, topic, marks, stem, options }
}

function formatCountdown(totalSeconds: number) {
  const s = Math.max(0, totalSeconds)
  const hrs = Math.floor(s / 3600)
  const mins = Math.floor((s % 3600) / 60)
  const secs = s % 60
  if (hrs > 0) {
    return `${String(hrs).padStart(2, "0")}:${String(mins).padStart(2, "0")}:${String(secs).padStart(2, "0")}`
  }
  return `${String(mins).padStart(2, "0")}:${String(secs).padStart(2, "0")}`
}

export default function StudentPortalScreen({
  network,
  initialCentreId,
  initialStudentRoll,
}: {
  network: NetworkState | null
  initialCentreId?: string
  initialStudentRoll?: string
}) {
  const allCentres = (network?.nodes || []).filter((n) => n.role === "Centre")
  const deliveredCentres = allCentres.filter((n) => n.delivered)
  const defaultCentre =
    initialCentreId ||
    deliveredCentres.find((c) => c.nodeId === "CTR-UK-01")?.nodeId ||
    deliveredCentres[0]?.nodeId ||
    "CTR-UK-01"

  const [centreId, setCentreId] = useState(defaultCentre)
  const [studentRoll, setStudentRoll] = useState(initialStudentRoll || "2510012263")
  const [portalData, setPortalData] = useState<StudentPortalResponse | null>(null)
  const [secondsLeft, setSecondsLeft] = useState<number>(0)
  const [loading, setLoading] = useState(false)
  const [err, setErr] = useState<string | null>(null)

  // Interactive Exam CBT Attempt State
  const [examStarted, setExamStarted] = useState(false)
  const [submitted, setSubmitted] = useState(false)
  const [currentIdx, setCurrentIdx] = useState(0)
  const [answers, setAnswers] = useState<Record<number, string>>({})
  const [markedReview, setMarkedReview] = useState<Record<number, boolean>>({})
  const [isFullscreen, setIsFullscreen] = useState<boolean>(
    () => typeof document !== "undefined" && Boolean(document.fullscreenElement),
  )
  const [exitWarnings, setExitWarnings] = useState(0)
  const [fullscreenError, setFullscreenError] = useState<string | null>(null)
  // Allocated exam countdown timer in seconds and auto-submit flag
  const [examSecondsLeft, setExamSecondsLeft] = useState<number>(2700)
  const [autoSubmittedFlag, setAutoSubmittedFlag] = useState<boolean>(false)

  const parsedQuestions: ParsedQuestion[] = (portalData?.shuffledQuestions || []).map(
    parseMcqQuestion,
  )
  const totalQ = parsedQuestions.length
  const answeredCount = Object.values(answers).filter((v) => v.trim().length > 0).length
  const reviewCount = Object.values(markedReview).filter(Boolean).length

  // Keeps latest counts accessible inside timers and exit callbacks without stale closures
  const latestMetricsRef = useRef({
    answeredCount: 0,
    reviewCount: 0,
    totalQ: 0,
    exitWarnings: 0,
    examSecondsLeft: 2700,
  })
  latestMetricsRef.current = {
    answeredCount,
    reviewCount,
    totalQ,
    exitWarnings,
    examSecondsLeft,
  }

  // Sends live progress, fullscreen exit warnings, or final submission to the backend
  const syncStatusToServer = (
    status: "Attempting" | "Submitted",
    autoSubmitted = false,
    overrideSeconds?: number,
  ) => {
    const snap = latestMetricsRef.current
    const activeCentre = portalData?.centreId || centreId
    const activeRoll = portalData?.studentRoll || studentRoll.trim() || "2510012263"
    void reportStudentStatus({
      centreId: activeCentre,
      studentRoll: activeRoll,
      status,
      answeredCount: snap.answeredCount,
      reviewCount: snap.reviewCount,
      totalQuestions: snap.totalQ,
      exitWarnings: snap.exitWarnings,
      timeRemainingSeconds:
        typeof overrideSeconds === "number" ? overrideSeconds : snap.examSecondsLeft,
      autoSubmitted,
    }).catch(() => {})
  }

  const fetchPortal = async (cId: string, roll: string) => {
    setLoading(true)
    setErr(null)
    try {
      const res = await queryStudentPortal({
        centreId: cId,
        studentRoll: roll.trim() || "2510012263",
        simulateUnlocked: false,
      })
      setPortalData(res)
      if (res.forwarded && res.locked && typeof res.secondsRemaining === "number") {
        setSecondsLeft(res.secondsRemaining)
      }
      if (!examStarted && !submitted && typeof res.examDurationSeconds === "number") {
        // Initialize student countdown clock from centre's allocated exam duration
        setExamSecondsLeft(res.examDurationSeconds)
      }
    } catch (e) {
      setErr(e instanceof Error ? e.message : "Unable to reach student portal.")
    } finally {
      setLoading(false)
    }
  }

  useEffect(() => {
    setExamStarted(false)
    setSubmitted(false)
    setAutoSubmittedFlag(false)
    setCurrentIdx(0)
    setAnswers({})
    setMarkedReview({})
    setExitWarnings(0)
    setFullscreenError(null)
    setExamSecondsLeft(2700)
    void fetchPortal(centreId, studentRoll)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [centreId, studentRoll])

  // Real-time countdown synced with Board's Min-Heap unlockTimestamp
  useEffect(() => {
    if (!portalData?.forwarded || !portalData?.locked) return
    const timer = window.setInterval(() => {
      setSecondsLeft((prev) => {
        if (prev <= 1) {
          void fetchPortal(centreId, studentRoll)
          return 0
        }
        return prev - 1
      })
    }, 1000)
    return () => window.clearInterval(timer)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [portalData?.forwarded, portalData?.locked, centreId, studentRoll])

  // Track mandatory browser Fullscreen state for proctored exam mode
  useEffect(() => {
    const onFsChange = () => {
      const active = Boolean(document.fullscreenElement)
      setIsFullscreen(active)
      if (!active && examStarted && !submitted) {
        setExitWarnings((w) => w + 1)
      }
    }
    document.addEventListener("fullscreenchange", onFsChange)
    return () => document.removeEventListener("fullscreenchange", onFsChange)
  }, [examStarted, submitted])

  // Live exam duration countdown timer inside fullscreen mode; auto-submits at 00:00
  useEffect(() => {
    if (!examStarted || submitted) return
    const timer = window.setInterval(() => {
      setExamSecondsLeft((prev) => {
        if (prev <= 1) {
          window.clearInterval(timer)
          void handleSubmitExam(true)
          return 0
        }
        return prev - 1
      })
    }, 1000)
    return () => window.clearInterval(timer)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [examStarted, submitted])

  // Immediately report answer changes, review flags, or fullscreen exit warnings to the centre
  useEffect(() => {
    if (!examStarted || submitted) return
    syncStatusToServer("Attempting", false)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [examStarted, submitted, answeredCount, reviewCount, exitWarnings])

  // Periodically heartbeat remaining timer seconds every 5s while attempting
  useEffect(() => {
    if (!examStarted || submitted) return
    const hb = window.setInterval(() => {
      syncStatusToServer("Attempting", false)
    }, 5000)
    return () => window.clearInterval(hb)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [examStarted, submitted])

  const enterFullscreen = async () => {
    setFullscreenError(null)
    try {
      if (!document.fullscreenElement) {
        await document.documentElement.requestFullscreen()
      }
      const initialDuration = examStarted
        ? examSecondsLeft
        : portalData?.examDurationSeconds || 2700
      if (!examStarted) {
        setExamSecondsLeft(initialDuration)
      }
      setIsFullscreen(true)
      setExamStarted(true)
      syncStatusToServer("Attempting", false, initialDuration)
    } catch {
      setFullscreenError(
        "Your browser blocked fullscreen mode. Please allow fullscreen to begin the examination.",
      )
    }
  }

  const handleSubmitExam = async (autoSubmitted = false) => {
    setSubmitted(true)
    setExamStarted(false)
    setAutoSubmittedFlag(autoSubmitted)
    syncStatusToServer("Submitted", autoSubmitted, autoSubmitted ? 0 : undefined)
    if (document.fullscreenElement) {
      try {
        await document.exitFullscreen()
      } catch {
        // ignore exit error
      }
    }
  }

  // =========================================================================
  // FULLSCREEN PROCTORED CBT INTERFACE (shown only once unlocked & started)
  // =========================================================================
  if (portalData?.forwarded && !portalData.locked && examStarted && !submitted && totalQ > 0) {
    if (!isFullscreen) {
      return (
        <div className="cbt-fullscreen-shell">
          <div className="cbt-fs-gate">
            <Card className="lock-card">
              <div className="icon-tile big">
                <Icon name="alert" />
              </div>
              <span className="eyebrow">PROCTORED SECURITY LOCK</span>
              <h2>Fullscreen Mode Required</h2>
              <p className="muted">
                This examination can only be viewed and attempted in full-screen mode. Exiting fullscreen pauses the question paper automatically.
              </p>
              {exitWarnings > 0 && (
                <div className="alert danger" style={{ marginTop: 16 }}>
                  <Icon name="alert" />
                  <span>
                    Fullscreen exit detected ({exitWarnings} warning{exitWarnings === 1 ? "" : "s"} recorded for Roll {portalData.studentRoll}).
                  </span>
                </div>
              )}
              <div style={{ marginTop: 24 }}>
                <Button onClick={() => void enterFullscreen()}>
                  Return to Fullscreen & Resume Exam <Icon name="arrow" />
                </Button>
              </div>
            </Card>
          </div>
        </div>
      )
    }

    const q = parsedQuestions[currentIdx] || parsedQuestions[0]
    const currentAns = answers[currentIdx] || ""
    const isMarked = Boolean(markedReview[currentIdx])

    return (
      <div className="cbt-fullscreen-shell">
        <header className="cbt-exam-topbar">
          <div className="cbt-top-left">
            <div className="brand">
              <Icon name="shield" />
              <span>Secure CBT · Live Examination</span>
            </div>
            <span className="badge ok">Fullscreen Active</span>
            {exitWarnings > 0 && (
              <span className="badge danger">FS Exits: {exitWarnings}</span>
            )}
          </div>
          <div className="cbt-top-right">
            <span className="mono muted">
              {portalData.centreId} · Roll {portalData.studentRoll}
            </span>
            <span className="badge neutral">
              Answered {answeredCount} / {totalQ}
            </span>
            {/* Live exam countdown clock synced with centre allocation */}
            <span
              className={`badge mono ${examSecondsLeft <= 60 ? "danger" : "neutral"}`}
              aria-live="polite"
            >
              Time Left: {formatCountdown(examSecondsLeft)}
            </span>
            <Button
              variant="secondary"
              className="compact-btn"
              onClick={() => void handleSubmitExam(false)}
            >
              Submit Exam
            </Button>
          </div>
        </header>

        <div className="cbt-exam-workspace">
          {/* LEFT: Single-question interactive MCQ canvas */}
          <Card className="cbt-question-pane">
            <div className="cbt-q-header">
              <div>
                <span className="eyebrow">
                  QUESTION {q.number} OF {totalQ} · {q.subject} / {q.topic}
                </span>
                <div className="mono muted small" style={{ marginTop: 4 }}>
                  Question ID: {q.id} · Seed: {portalData.shuffleSeed}
                </div>
              </div>
              <div className="btn-row">
                {isMarked && <span className="badge warn">Marked for Review</span>}
                {q.marks && <span className="badge ok">{q.marks} marks</span>}
              </div>
            </div>

            <div className="cbt-q-stem">{q.stem}</div>

            {q.options.length > 0 ? (
              <div className="cbt-options" role="radiogroup" aria-label={`Options for Question ${q.number}`}>
                {q.options.map((opt) => {
                  const selected = currentAns === opt.label
                  return (
                    <button
                      key={opt.label}
                      type="button"
                      role="radio"
                      aria-checked={selected}
                      className={`cbt-option ${selected ? "selected" : ""}`}
                      onClick={() =>
                        setAnswers((prev) => ({ ...prev, [currentIdx]: opt.label }))
                      }
                    >
                      <span className="cbt-opt-letter">{opt.label}</span>
                      <span className="cbt-opt-text">{opt.text}</span>
                      {selected && <Icon name="check" />}
                    </button>
                  )
                })}
              </div>
            ) : (
              <div className="cbt-subjective">
                <label className="field">
                  <span className="eyebrow">YOUR RESPONSE</span>
                  <textarea
                    rows={5}
                    value={currentAns}
                    onChange={(e) =>
                      setAnswers((prev) => ({ ...prev, [currentIdx]: e.target.value }))
                    }
                    placeholder="Type your response here…"
                  />
                </label>
              </div>
            )}

            <div className="cbt-q-footer">
              <div className="btn-row">
                <Button
                  variant="secondary"
                  className="compact-btn"
                  disabled={currentIdx === 0}
                  onClick={() => setCurrentIdx((i) => Math.max(0, i - 1))}
                >
                  ← Previous
                </Button>
                <Button
                  variant="secondary"
                  className="compact-btn"
                  disabled={!currentAns}
                  onClick={() =>
                    setAnswers((prev) => {
                      const next = { ...prev }
                      delete next[currentIdx]
                      return next
                    })
                  }
                >
                  Clear Response
                </Button>
                <Button
                  variant="secondary"
                  className="compact-btn"
                  onClick={() =>
                    setMarkedReview((prev) => ({
                      ...prev,
                      [currentIdx]: !prev[currentIdx],
                    }))
                  }
                >
                  {isMarked ? "Unmark Review" : "Mark for Review"}
                </Button>
              </div>

              {currentIdx < totalQ - 1 ? (
                <Button
                  onClick={() => setCurrentIdx((i) => Math.min(totalQ - 1, i + 1))}
                >
                  Save & Next <Icon name="arrow" />
                </Button>
              ) : (
                <Button onClick={() => void handleSubmitExam()}>
                  Finish & Submit Exam <Icon name="check" />
                </Button>
              )}
            </div>
          </Card>

          {/* RIGHT: Question Palette / Jump Bar */}
          <Card className="cbt-palette-pane">
            <div className="card-heading">
              <span className="eyebrow">QUESTION PALETTE</span>
              <h3>Jump to Question</h3>
            </div>

            <div className="cbt-legend">
              <span>
                <i className="pal-dot answered" /> Answered ({answeredCount})
              </span>
              <span>
                <i className="pal-dot review" /> Review ({reviewCount})
              </span>
              <span>
                <i className="pal-dot unanswered" /> Unanswered ({totalQ - answeredCount})
              </span>
            </div>

            <div className="cbt-palette-grid">
              {parsedQuestions.map((item, idx) => {
                const hasAns = Boolean((answers[idx] || "").trim())
                const hasRev = Boolean(markedReview[idx])
                const cls = [
                  "pal-btn",
                  idx === currentIdx ? "current" : "",
                  hasRev ? "review" : hasAns ? "answered" : "unanswered",
                ]
                  .filter(Boolean)
                  .join(" ")
                return (
                  <button
                    key={item.id}
                    type="button"
                    className={cls}
                    onClick={() => setCurrentIdx(idx)}
                    title={`Question ${idx + 1} (${item.id})`}
                  >
                    {idx + 1}
                  </button>
                )
              })}
            </div>

            <div className="cbt-palette-footer">
              <Button onClick={() => void handleSubmitExam()}>
                Submit Examination <Icon name="check" />
              </Button>
            </div>
          </Card>
        </div>
      </div>
    )
  }

  // =========================================================================
  // NORMAL STUDENT TERMINAL LOBBY (Time-Lock Countdown / Pre-Start / Submitted)
  // =========================================================================
  return (
    <div className="phase-stack">
      <Card>
        <div className="card-heading">
          <span className="eyebrow">STUDENT EXAM TERMINAL</span>
          <h2>Candidate Paper Access</h2>
          <p className="muted">
            Your terminal verifies the Board&apos;s Min-Heap time-lock, unwraps 6 AES-256-GCM onion layers, and launches fullscreen MCQ mode once exam start time arrives.
          </p>
        </div>

        <div className="inline-form">
          <label className="field grow">
            <span className="eyebrow">EXAM CENTRE</span>
            <select
              value={centreId}
              disabled={loading}
              onChange={(e) => setCentreId(e.target.value)}
            >
              {(deliveredCentres.length > 0 ? deliveredCentres : allCentres).map((c) => (
                <option key={c.nodeId} value={c.nodeId}>
                  {c.nodeId} — {c.name}
                </option>
              ))}
            </select>
          </label>

          <label className="field">
            <span className="eyebrow">STUDENT ROLL NO.</span>
            <input
              type="text"
              value={studentRoll}
              disabled={loading}
              onChange={(e) => setStudentRoll(e.target.value)}
              placeholder="2510012263"
            />
          </label>

          <Button
            variant="secondary"
            disabled={loading}
            onClick={() => void fetchPortal(centreId, studentRoll)}
          >
            {loading ? "Checking…" : "Refresh"}
          </Button>
        </div>

        <div className="preset-row" style={{ marginTop: 14 }}>
          <span className="muted small">Quick switch roll no.:</span>
          {[
            { id: "2510012263", label: "2510012263 (Kushagra)" },
            { id: "2510013438", label: "2510013438 (Naman)" },
            { id: "2510370185", label: "2510370185 (Aayush)" },
          ].map((stu) => (
            <Button
              key={stu.id}
              variant={studentRoll === stu.id ? "primary" : "secondary"}
              className="compact-btn"
              disabled={loading}
              onClick={() => setStudentRoll(stu.id)}
            >
              {stu.label}
            </Button>
          ))}
        </div>
      </Card>

      {err && (
        <Card>
          <p className="danger">{err}</p>
        </Card>
      )}

      {portalData && !portalData.forwarded && (
        <Card className="lock-card">
          <div className="icon-tile big">
            <Icon name="lock" />
          </div>
          <h2>Awaiting Exam Centre Forwarding</h2>
          <p className="muted">
            {portalData.message ||
              `Roll ${portalData.studentRoll} at ${portalData.centreId} has not received a forwarded paper yet.`}
          </p>
        </Card>
      )}

      {portalData && portalData.forwarded && portalData.locked && (
        <Card className="lock-card">
          <div className="icon-tile big">
            <Icon name="lock" />
          </div>
          <span className="eyebrow">TIME-LOCKED PACKAGE RECEIVED</span>
          <h2>
            Encrypted Paper Received — Unlocks at{" "}
            {portalData.unlockTimestamp
              ? new Date(portalData.unlockTimestamp * 1000).toLocaleString()
              : portalData.unlockTimeFormatted}
          </h2>
          <div className="countdown-display" aria-live="polite">
            {formatCountdown(secondsLeft)}
          </div>
          <p className="muted small">
            Questions remain AES-256-GCM encrypted in terminal memory and will unlock automatically when the countdown reaches 00:00.
          </p>

          <div className="stats compact-stats" style={{ marginTop: 20 }}>
            <Stat
              label="Encrypted Package"
              value={`${(portalData.encryptedBytes || 0).toLocaleString()} B`}
              note={`${portalData.onionLayers || 6} onion layers`}
            />
            <span className="stat-arrow">·</span>
            <Stat
              label="Terminal Key Fingerprint"
              value={portalData.keyFingerprint || "—"}
              note={portalData.studentNodeId || portalData.studentRoll}
            />
          </div>
        </Card>
      )}

      {portalData && portalData.forwarded && !portalData.locked && submitted && (
        <Card className="lock-card">
          <div className="icon-tile big">
            <Icon name="check" />
          </div>
          <span className="eyebrow">
            {autoSubmittedFlag
              ? "EXAMINATION AUTO-SUBMITTED (TIME EXPIRED)"
              : "EXAMINATION SUBMITTED"}
          </span>
          <h2>Responses Recorded — Roll {portalData.studentRoll}</h2>
          <p className="muted">
            {autoSubmittedFlag
              ? `The exam timer allocated by ${portalData.centreId} reached 00:00 and your answers were submitted automatically.`
              : `Your answers at ${portalData.centreId} have been submitted cleanly.`}
          </p>
          <div className="stats compact-stats" style={{ marginTop: 20 }}>
            <Stat label="Answered" value={`${answeredCount} / ${totalQ}`} note="Saved responses" />
            <span className="stat-arrow">·</span>
            <Stat label="Marked for Review" value={`${reviewCount}`} note="Flagged items" />
            <span className="stat-arrow">·</span>
            <Stat
              label="Proctor Alerts"
              value={`${exitWarnings}`}
              note="Fullscreen exits"
            />
          </div>
          {!autoSubmittedFlag && examSecondsLeft > 0 && (
            <div style={{ marginTop: 24 }}>
              <Button
                variant="secondary"
                onClick={() => {
                  setSubmitted(false)
                  void enterFullscreen()
                }}
              >
                Re-open in Fullscreen
              </Button>
            </div>
          )}
        </Card>
      )}

      {portalData && portalData.forwarded && !portalData.locked && !examStarted && !submitted && (
        <Card className="lock-card">
          <div className="icon-tile big">
            <Icon name="check" />
          </div>
          <span className="eyebrow">TIME-LOCK RELEASED · CRYPTO VERIFIED</span>
          <h2>Paper Unwrapped & Ready — Roll {portalData.studentRoll}</h2>
          <p className="muted">
            All {portalData.layersPeeled || 6} AES-256-GCM onion layers verified and Huffman-decompressed. Fullscreen mode is mandatory to view and attempt questions.
          </p>

          <div className="stats compact-stats" style={{ marginTop: 20 }}>
            <Stat
              label="Onion Layers Peeled"
              value={`${portalData.layersPeeled || 6} / 6`}
              note="Student → Board"
            />
            <span className="stat-arrow">·</span>
            <Stat
              label="Questions Ready"
              value={`${totalQ} MCQs`}
              note={`Shuffle Seed ${portalData.shuffleSeed}`}
            />
            <span className="stat-arrow">·</span>
            <Stat
              label="Exam Timer"
              value={`${Math.max(1, Math.round((portalData.examDurationSeconds || 2700) / 60))} mins`}
              note="Auto-submits at 00:00"
            />
            <span className="stat-arrow">·</span>
            <Stat
              label="Decompressed Size"
              value={`${(portalData.plainBytes || 0).toLocaleString()} B`}
              note={`From ${(portalData.compressedBytes || 0).toLocaleString()} B`}
            />
          </div>

          {fullscreenError && (
            <div className="alert danger" style={{ marginTop: 16 }}>
              <Icon name="alert" />
              <span>{fullscreenError}</span>
            </div>
          )}

          <div style={{ marginTop: 24 }}>
            <Button onClick={() => void enterFullscreen()}>
              Enter Fullscreen & Start Examination <Icon name="arrow" />
            </Button>
          </div>
        </Card>
      )}
    </div>
  )
}
