import type { Paper } from "../types"
import Card from "../components/Card"
import Button from "../components/Button"
import Icon from "../components/Icon"
import { USE_MOCK } from "../api"
export default function PreviewScreen({
  paper,
  onBack,
  onLock,
}: {
  paper: Paper
  onBack: () => void
  onLock: () => void
}) {
  return (
    <>
      <div className="preview-summary">
        <span className="eyebrow">PAPER OVERVIEW</span>
        <h2>
          {paper.totalQuestions} questions · {paper.totalMarks} marks
        </h2>
      </div>
      <Card className="paper-card">
        <ol className="questions">
          {paper.questions.map((q) => (
            <li key={q.number}>
              <span className="question-number">
                {String(q.number).padStart(2, "0")}
              </span>
              <div className="question-body">
                <div className="question-meta">
                  <div className="question-context">
                    <h3>{q.subject}</h3>
                    <span className="question-topic">{q.topic}</span>
                  </div>
                  <span className="marks-badge">
                    {q.marks} {q.marks === 1 ? "mark" : "marks"}
                  </span>
                </div>
                <p>{q.text}</p>
              </div>
            </li>
          ))}
        </ol>
      </Card>
      <details className="preview-disclosure">
        <summary>
          <Icon name="alert" />
          <span>About this preview</span>
          <Icon name="chevron" />
        </summary>
        <p>
          This preview is shown for Phase 1 verification before locking and
          encryption. The paper is never downloadable unencrypted.
        </p>
      </details>
      <div className="actions page-actions">
        <div className="primary-slot">
          <Button onClick={onLock}>
            <Icon name="lock" />
            Lock & encrypt
          </Button>
        </div>
        <Button variant="secondary" onClick={onBack}>
          Back to configure
        </Button>
      </div>
    </>
  )
}
