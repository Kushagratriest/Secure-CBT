import type { BankSummary, Selection } from "../types"
import Card from "../components/Card"
import Button from "../components/Button"
import TopicCard from "../components/TopicCard"
import Icon from "../components/Icon"
export default function ConfigureScreen({
  summary,
  selections,
  busy,
  onChange,
  onBack,
  onGenerate,
}: {
  summary: BankSummary
  selections: Selection[]
  busy: boolean
  onChange: (s: Selection) => void
  onBack: () => void
  onGenerate: () => void
}) {
  const count = selections.reduce((n, s) => n + s.count, 0)
  const marks = selections.reduce((n, s) => n + s.count * s.marks, 0)
  return (
    <>
      <Card className="selection-summary">
        <div>
          <p className="eyebrow">YOUR QUESTION BANK</p>
          <h2>Total questions in bank: {summary.totalQuestions}</h2>
        </div>
        <p className="live-total" aria-live="polite">
          Selected:{" "}
          <strong>
            {count} questions · {marks} marks
          </strong>
        </p>
      </Card>
      <div className="section-label">
        <h2>Select your questions</h2>
        <span className="muted small">{summary.subjects.length} subjects</span>
      </div>
      <div className={busy ? "subjects is-busy" : "subjects"}>
        {summary.subjects.map((subject) => (
          <details className="card subject-card" key={subject.name} open>
            <summary>
              <div>
                <h2>{subject.name}</h2>
                <span className="muted small">{subject.total} questions</span>
              </div>
              <Icon name="chevron" />
            </summary>
            <div className="subject-content">
              {subject.topics.map((topic) => (
                <TopicCard
                  key={topic.name}
                  topic={topic}
                  subject={subject.name}
                  selections={selections}
                  onChange={onChange}
                />
              ))}
            </div>
          </details>
        ))}
      </div>
      <div className="actions page-actions">
        <div className="primary-slot">
          <Button disabled={!count || busy} onClick={onGenerate}>
            {busy ? (
              <>
                <span className="spinner" /> Generating paper…
              </>
            ) : (
              <>
                Generate paper <Icon name="arrow" />
              </>
            )}
          </Button>
        </div>
        <Button variant="secondary" onClick={onBack} disabled={busy}>
          Back
        </Button>
      </div>
    </>
  )
}
