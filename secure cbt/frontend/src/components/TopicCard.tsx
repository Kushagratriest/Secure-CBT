import type { Selection, Topic } from "../types"
import Counter from "./Counter"
import { useId } from "react"
export default function TopicCard({
  topic,
  subject,
  selections,
  onChange,
}: {
  topic: Topic
  subject: string
  selections: Selection[]
  onChange: (s: Selection) => void
}) {
  const headingId = useId()
  const selected = selections
    .filter((s) => s.subject === subject && s.topic === topic.name)
    .reduce((total, s) => total + s.count, 0)
  return (
    <section className="topic" aria-labelledby={headingId}>
      <div className="topic-heading">
        <h3 id={headingId}>{topic.name}</h3>
        <span
          className={`topic-selection ${selected > 0 ? "has-selection" : ""}`}
        >
          {selected} of {topic.total} selected
        </span>
      </div>
      <div className="topic-groups">
        {topic.marksGroups.map((g) => (
          <div className="group-row" key={g.marks}>
            <p>
              <strong>{g.marks}-mark questions</strong>
              <span className="muted"> · {g.available} available</span>
            </p>
            <Counter
              label={`${subject}, ${topic.name}, ${g.marks}-mark questions`}
              max={g.available}
              value={
                selections.find(
                  (s) =>
                    s.subject === subject &&
                    s.topic === topic.name &&
                    s.marks === g.marks,
                )?.count || 0
              }
              onChange={(count) =>
                onChange({ subject, topic: topic.name, marks: g.marks, count })
              }
            />
          </div>
        ))}
      </div>
    </section>
  )
}
