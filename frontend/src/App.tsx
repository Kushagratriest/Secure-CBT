import { useEffect, useState } from "react"
import {
  forwardToStudents,
  generatePaper,
  getPhase2Network,
  lockPaper,
  runPhase2Broadcast,
  scheduleExamUnlock,
  // Calls server to allocate per-centre exam duration
  setCentreDuration,
  setMockIntegrityFailure,
  traceLeakedPaper,
  triggerAuditAction,
  uploadCsv,
  USE_MOCK,
} from "./api"
import type {
  CentreForwardReceipt,
  LeakTraceReport,
  LockResult,
  NetworkState,
  Paper,
  Portal,
  Screen,
  Selection,
  UploadResult,
} from "./types"
import Alert from "./components/Alert"
import Button from "./components/Button"
import Icon from "./components/Icon"
import Stepper from "./components/Stepper"
import UploadScreen from "./screens/UploadScreen"
import ConfigureScreen from "./screens/ConfigureScreen"
import PreviewScreen from "./screens/PreviewScreen"
import LockedScreen from "./screens/LockedScreen"
import DistributionScreen from "./screens/DistributionScreen"
import ExamCentreScreen from "./screens/ExamCentreScreen"
import StudentPortalScreen from "./screens/StudentPortalScreen"

const headings = [
  "Upload Question Bank",
  "Configure your paper",
  "Review your paper",
  "Lock & Schedule",
  "National Broadcast",
]
const subtitles = [
  "Upload your CSV file. The system will validate it before building your question pool.",
  "Choose the right balance of subjects, topics, and marks for your exam.",
  "One final look before your paper is compressed and encrypted.",
  "Set the Min-Heap exam unlock date & time and seal the encrypted package.",
  "Broadcast uniquely keyed AES-256-GCM onion layers across all 36 Indian States & UTs.",
]
const phaseEyebrows = [
  "CENTRAL BOARD · PAPER SETTING",
  "CENTRAL BOARD · PAPER SETTING",
  "CENTRAL BOARD · PAPER SETTING",
  "CENTRAL BOARD · LOCK & SCHEDULE",
  "CENTRAL BOARD · NATIONAL BROADCAST",
]

export default function App() {
  const [portal, setPortal] = useState<Portal>("board")
  const [screen, setScreen] = useState<Screen>(0)
  const [file, setFile] = useState<File | null>(null)
  const [upload, setUpload] = useState<UploadResult | null>(null)
  const [selections, setSelections] = useState<Selection[]>([])
  const [paper, setPaper] = useState<Paper | null>(null)
  const [locked, setLocked] = useState<LockResult | null>(null)
  const [network, setNetwork] = useState<NetworkState | null>(null)
  const [receipts, setReceipts] = useState<CentreForwardReceipt[]>([])
  const [leakReport, setLeakReport] = useState<LeakTraceReport | null>(null)
  const [studentTarget, setStudentTarget] = useState<{ centreId?: string; roll?: string }>({})
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState<Error | null>(null)
  const [integrityFailure, setIntegrityFailure] = useState(false)

  useEffect(() => {
    void getPhase2Network()
      .then((n) => setNetwork(n))
      .catch(() => {})
  }, [])

  const navigate = (s: Screen) => {
    setError(null)
    setScreen(s)
    window.scrollTo({ top: 0, behavior: "instant" })
  }
  const run = async (task: () => Promise<void>) => {
    if (busy) return
    setBusy(true)
    setError(null)
    try {
      await task()
    } catch (e) {
      setError(
        e instanceof Error
          ? e
          : new Error("Something went wrong. Please try again."),
      )
    } finally {
      setBusy(false)
    }
  }
  const selectFile = (f: File) => {
    setFile(f)
    setUpload(null)
    setSelections([])
    setPaper(null)
    setLocked(null)
    setError(null)
  }
  const lock = () => {
    setLocked(null)
    setReceipts([])
    setLeakReport(null)
    navigate(3)
    void run(async () => {
      setMockIntegrityFailure(integrityFailure)
      setLocked(await lockPaper())
      setNetwork(await getPhase2Network())
    })
  }
  const openDistribution = () => {
    setPortal("board")
    navigate(4)
    void run(async () => {
      setNetwork(await getPhase2Network())
    })
  }
  const openExamCentre = () => {
    setError(null)
    setPortal("centre")
    window.scrollTo({ top: 0, behavior: "instant" })
  }
  const reset = () => {
    setFile(null)
    setUpload(null)
    setSelections([])
    setPaper(null)
    setLocked(null)
    setReceipts([])
    setLeakReport(null)
    setIntegrityFailure(false)
    setMockIntegrityFailure(false)
    setPortal("board")
    navigate(0)
  }

  const headerPhaseLabel =
    screen <= 3
      ? "Phase 1 — Paper Setting"
      : screen === 4
        ? "Phase 2 — Secure Distribution"
        : "Phase 3 — Exam Centre & Traceability"

  return (
    <div className="app">
      <header className="site-header">
        <div className="header-inner">
          <a
            className="brand"
            href="/"
            onClick={(e) => {
              e.preventDefault()
              if (!busy && screen !== 3) navigate(0)
            }}
          >
            <Icon name="shield" />
            <span>Secure CBT</span>
          </a>
          <div className="header-right">
            <nav className="role-toggle" aria-label="Role Portals">
              {[
                { id: "board" as Portal, label: "1. Central Board" },
                { id: "centre" as Portal, label: "2. Exam Centre" },
                { id: "student" as Portal, label: "3. Student Terminal" },
              ].map((p) => (
                <button
                  key={p.id}
                  type="button"
                  className={`role-btn ${portal === p.id ? "active" : ""}`}
                  disabled={busy}
                  onClick={() => {
                    setError(null)
                    setPortal(p.id)
                  }}
                >
                  {p.label}
                </button>
              ))}
            </nav>
          </div>
        </div>
      </header>
      <main className="main">
        {portal === "board" && (
          <>
            <Stepper
              current={screen}
              busy={busy}
              canJumpToDistribute={Boolean(network?.hasPackage)}
              onBack={(s) => {
                if (!busy && s < screen) navigate(s)
              }}
              onJump={(s) => {
                if (!busy && s === 4) openDistribution()
              }}
            />
            <div className="page-heading">
              <span className="eyebrow">
                {phaseEyebrows[screen]} / STEP {String(screen + 1).padStart(2, "0")}
              </span>
              <h1>{headings[screen]}</h1>
              <p>{subtitles[screen]}</p>
            </div>
          </>
        )}
        <div className="screen-content">
          <Alert error={error} />
          {portal === "board" && screen === 0 && (
            <UploadScreen
              file={file}
              result={upload}
              busy={busy}
              integrityFailure={integrityFailure}
              onIntegrityFailure={setIntegrityFailure}
              onFile={selectFile}
              onUpload={() => {
                if (file) void run(async () => setUpload(await uploadCsv(file)))
              }}
              onContinue={() => {
                if (upload?.canContinue && upload.summary) navigate(1)
              }}
            />
          )}
          {portal === "board" && screen === 1 && upload?.summary && (
            <ConfigureScreen
              summary={upload.summary}
              selections={selections}
              busy={busy}
              onChange={(s) =>
                setSelections((previous) => [
                  ...previous.filter(
                    (p) =>
                      !(
                        p.subject === s.subject &&
                        p.topic === s.topic &&
                        p.marks === s.marks
                      ),
                  ),
                  s,
                ])
              }
              onBack={() => navigate(0)}
              onGenerate={() =>
                void run(async () => {
                  setPaper(await generatePaper(selections))
                  setLocked(null)
                  navigate(2)
                })
              }
            />
          )}
          {portal === "board" && screen === 2 && paper && (
            <PreviewScreen
              paper={paper}
              onBack={() => navigate(1)}
              onLock={lock}
            />
          )}
          {portal === "board" && screen === 3 && (
            <LockedScreen
              result={locked}
              busy={busy}
              onReset={reset}
              onRetry={lock}
              onBack={() => navigate(2)}
              onSchedule={async (unlockTimestamp) => {
                await run(async () => {
                  const updated = await scheduleExamUnlock({ unlockTimestamp })
                  setNetwork(updated)
                  setLocked((prev) =>
                    prev
                      ? {
                          ...prev,
                          unlockTimestamp: updated.unlockTimestamp,
                          unlockTimeFormatted: updated.unlockTimeFormatted,
                        }
                      : prev,
                  )
                })
              }}
              onDistribute={openDistribution}
            />
          )}
          {portal === "board" && screen === 4 && (
            <DistributionScreen
              network={network}
              leakReport={leakReport}
              busy={busy}
              onRefresh={() =>
                void run(async () => setNetwork(await getPhase2Network()))
              }
              onSchedule={(payload) =>
                void run(async () => setNetwork(await scheduleExamUnlock(payload)))
              }
              onBroadcast={() =>
                void run(async () => {
                  setReceipts([])
                  setLeakReport(null)
                  setNetwork(await runPhase2Broadcast())
                })
              }
              onAuditAction={(action, index = 1) =>
                void run(async () => {
                  const audit = await triggerAuditAction(action, index)
                  setNetwork((prev) => (prev ? { ...prev, audit } : prev))
                })
              }
              onTraceLeak={(nodeId) =>
                void run(async () => setLeakReport(await traceLeakedPaper(nodeId)))
              }
              onContinuePhase3={openExamCentre}
              onBack={() => navigate(locked ? 3 : 0)}
            />
          )}
          {portal === "centre" && (
            <ExamCentreScreen
              network={network}
              receipts={receipts}
              busy={busy}
              onForward={(payload) =>
                void run(async () => {
                  const res = await forwardToStudents(payload)
                  setReceipts(res.receipts)
                  setNetwork(res.network)
                })
              }
              onAllocateDuration={(centreId, examDurationSeconds) =>
                void run(async () => {
                  // Save allocated duration for this centre on C++ backend
                  setNetwork(await setCentreDuration({ centreId, examDurationSeconds }))
                })
              }
              onRefreshNetwork={() => {
                // Silently poll server for live student attempt & fullscreen warnings
                void getPhase2Network()
                  .then((n) => setNetwork(n))
                  .catch(() => {})
              }}
              onOpenStudentTerminal={(centreId, roll) => {
                setStudentTarget({ centreId, roll })
                setPortal("student")
                window.scrollTo({ top: 0, behavior: "instant" })
              }}
              onBackToBoard={() => setPortal("board")}
            />
          )}
          {portal === "student" && (
            <StudentPortalScreen
              network={network}
              initialCentreId={studentTarget.centreId}
              initialStudentRoll={studentTarget.roll}
            />
          )}
        </div>
        <footer className="site-footer">
          <Icon name="shield" />
          <span>Secure by design. Built for confident exams.</span>
          {USE_MOCK && <span className="footer-demo">Local demo</span>}
        </footer>
      </main>
    </div>
  )
}
