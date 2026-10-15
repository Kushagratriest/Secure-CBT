import { useState } from "react"
import {
  generatePaper,
  lockPaper,
  setMockIntegrityFailure,
  uploadCsv,
  USE_MOCK,
} from "./api"
import type {
  LockResult,
  Paper,
  Screen,
  Selection,
  UploadResult,
} from "./types"
import Alert from "./components/Alert"
import Icon from "./components/Icon"
import Stepper from "./components/Stepper"
import UploadScreen from "./screens/UploadScreen"
import ConfigureScreen from "./screens/ConfigureScreen"
import PreviewScreen from "./screens/PreviewScreen"
import LockedScreen from "./screens/LockedScreen"
const headings = [
  "Upload Question Bank",
  "Configure your paper",
  "Review your paper",
  "A secure finish.",
]
const subtitles = [
  "Upload your CSV file. The system will validate it before building your question pool.",
  "Choose the right balance of subjects, topics, and marks for your exam.",
  "One final look before your paper is compressed and encrypted.",
  "From question paper to protected package, in one step.",
]
export default function App() {
  const [screen, setScreen] = useState<Screen>(0)
  const [file, setFile] = useState<File | null>(null)
  const [upload, setUpload] = useState<UploadResult | null>(null)
  const [selections, setSelections] = useState<Selection[]>([])
  const [paper, setPaper] = useState<Paper | null>(null)
  const [locked, setLocked] = useState<LockResult | null>(null)
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState<Error | null>(null)
  const [integrityFailure, setIntegrityFailure] = useState(false)
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
    navigate(3)
    void run(async () => {
      setMockIntegrityFailure(integrityFailure)
      setLocked(await lockPaper())
    })
  }
  const reset = () => {
    setFile(null)
    setUpload(null)
    setSelections([])
    setPaper(null)
    setLocked(null)
    setIntegrityFailure(false)
    setMockIntegrityFailure(false)
    navigate(0)
  }
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
          <span className="phase-label">Phase 1 — Paper Setting</span>
        </div>
      </header>
      <main className="main">
        <Stepper
          current={screen}
          busy={busy}
          onBack={(s) => {
            if (!busy && s < screen) navigate(s)
          }}
        />
        <div className="page-heading">
          <span className="eyebrow">
            PAPER SETTING / STEP {String(screen + 1).padStart(2, "0")}
          </span>
          <h1>{headings[screen]}</h1>
          <p>{subtitles[screen]}</p>
        </div>
        <div className="screen-content">
          <Alert error={error} />
          {screen === 0 && (
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
          {screen === 1 && upload?.summary && (
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
          {screen === 2 && paper && (
            <PreviewScreen
              paper={paper}
              onBack={() => navigate(1)}
              onLock={lock}
            />
          )}
          {screen === 3 && (
            <LockedScreen
              result={locked}
              busy={busy}
              onReset={reset}
              onRetry={lock}
              onBack={() => navigate(2)}
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
