import { columns, hasValidHeaders, parseCSV } from "./csv"
import type {
  BankSummary,
  ErrorResponse,
  LockResult,
  Paper,
  Question,
  Selection,
  UploadResult,
  ValidationError,
} from "./types"

export const USE_MOCK = false
export class ApiError extends Error {
  constructor(
    message: string,
    public problems: string[] = [],
  ) {
    super(message)
  }
}
let bank: Question[] = []
let generated: Paper | null = null
let mockIntegrityFailure = false
export function setMockIntegrityFailure(value: boolean) {
  mockIntegrityFailure = value
}
const delay = () => new Promise((resolve) => setTimeout(resolve, 850))
async function request<T>(path: string, options: RequestInit): Promise<T> {
  let response: Response
  try {
    response = await fetch(path, options)
  } catch {
    throw new ApiError(
      "Unable to reach the server. Check your connection and try again.",
    )
  }
  let data: T & Partial<ErrorResponse>
  try {
    data = await response.json()
  } catch {
    throw new ApiError(
      "The server returned an unreadable response. Please try again.",
    )
  }
  if (!response.ok || data.error)
    throw new ApiError(
      data.error || `The request could not be completed (${response.status}).`,
      data.problems || [],
    )
  return data
}
function summarize(questions: Question[]): BankSummary {
  const subjects: BankSummary["subjects"] = []
  for (const q of questions) {
    let subject = subjects.find((s) => s.name === q.subject)
    if (!subject) {
      subject = { name: q.subject, total: 0, topics: [] }
      subjects.push(subject)
    }
    subject.total++
    let topic = subject.topics.find((t) => t.name === q.topic)
    if (!topic) {
      topic = { name: q.topic, total: 0, marksGroups: [] }
      subject.topics.push(topic)
    }
    topic.total++
    let group = topic.marksGroups.find((g) => g.marks === q.marks)
    if (!group) {
      group = { marks: q.marks, available: 0 }
      topic.marksGroups.push(group)
    }
    group.available++
  }
  return { totalQuestions: questions.length, subjects }
}
export async function uploadCsv(file: File): Promise<UploadResult> {
  let text: string
  try {
    text = new TextDecoder("utf-8", { fatal: true }).decode(
      await file.arrayBuffer(),
    )
  } catch {
    throw new ApiError(
      "Save your CSV using UTF-8 encoding and upload it again.",
    )
  }
  if (!USE_MOCK)
    return request("/api/upload", {
      method: "POST",
      headers: { "Content-Type": "text/csv", "X-Filename": file.name },
      body: text,
    })
  await delay()
  bank = []
  generated = null
  if (file.name === "demo-server-error.csv")
    throw new ApiError("The server could not process this upload.", [
      "The question-bank service is temporarily unavailable.",
      "Please try uploading again.",
    ])
  const errors: ValidationError[] = []
  const warnings: string[] = []
  try {
    if (!file.name.toLowerCase().endsWith(".csv"))
      throw new Error("Please select a .csv file.")
    const rows = parseCSV(text)
    if (!hasValidHeaders(rows[0]?.values))
      throw new Error(`Expected these columns in order: ${columns.join(", ")}.`)
    const ids = new Set<string>()
    for (const { row, values } of rows.slice(1)) {
      if (values.length !== 6)
        errors.push({ row, field: "Columns", problem: "Expected six fields." })
      columns.forEach((field, i) => {
        if (!values[i])
          errors.push({ row, field, problem: "Required field is missing." })
      })
      for (const i of [4, 5])
        if (
          values[i] &&
          (!/^\d+(\.\d+)?$/.test(values[i]) ||
            !Number.isFinite(Number(values[i])) ||
            Number(values[i]) <= 0 ||
            (i === 5 && !Number.isInteger(Number(values[i]))))
        )
          errors.push({
            row,
            field: columns[i],
            problem:
              i === 5
                ? `marks must be a whole number greater than 0 (got "${values[i]}").`
                : "Use a positive number.",
          })
      if (ids.has(values[0])) {
        warnings.push(
          `Duplicate question_id "${values[0]}" skipped (first one kept)`,
        )
        continue
      }
      ids.add(values[0])
      bank.push({
        number: bank.length + 1,
        id: values[0],
        text: values[1],
        subject: values[2],
        topic: values[3],
        marks: Number(values[5]),
      })
    }
  } catch (error) {
    errors.push({
      row: 0,
      field: "File",
      problem: error instanceof Error ? error.message : "Unable to read CSV.",
    })
  }
  if (errors.length) {
    bank = []
    return {
      filename: file.name,
      validCount: 0,
      isValid: false,
      isEmptyBank: false,
      canContinue: false,
      errors,
    }
  }
  if (!bank.length)
    return {
      filename: file.name,
      validCount: 0,
      isValid: true,
      isEmptyBank: true,
      canContinue: false,
      errors: [],
    }
  return {
    filename: file.name,
    validCount: bank.length,
    isValid: true,
    isEmptyBank: false,
    canContinue: true,
    errors: [],
    warnings,
    summary: summarize(bank),
  }
}
export async function generatePaper(selections: Selection[]): Promise<Paper> {
  const chosen = selections
    .filter((s) => s.count > 0)
    .map((s) => ({ ...s, marks: Number(s.marks), count: Number(s.count) }))
  if (!USE_MOCK)
    return request("/api/generate", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ selections: chosen }),
    })
  await delay()
  if (!chosen.length)
    throw new ApiError("Select at least one question to generate a paper.")
  const questions = chosen
    .flatMap((s) =>
      bank
        .filter(
          (q) =>
            q.subject === s.subject &&
            q.topic === s.topic &&
            q.marks === s.marks,
        )
        .slice(0, s.count),
    )
    .map((q, i) => ({ ...q, number: i + 1 }))
  generated = {
    totalQuestions: questions.length,
    totalMarks: questions.reduce((n, q) => n + q.marks, 0),
    questions,
    paperText: questions
      .map((q) => `${q.number}. ${q.text} [${q.marks} marks]`)
      .join("\n\n"),
  }
  return generated
}
export async function lockPaper(): Promise<LockResult> {
  if (!USE_MOCK) return request("/api/lock", { method: "POST" })
  await delay()
  if (!generated) throw new ApiError("Generate a paper before locking it.")
  const originalBits = new TextEncoder().encode(generated.paperText).length * 8
  const compressedBits = Math.round(originalBits * 0.61)
  return {
    originalBits,
    compressedBits,
    compressedBytes: Math.ceil(compressedBits / 8),
    encryptedBytes: Math.ceil(compressedBits / 8),
    packageFile: "output/paper.scbt",
    verified: !mockIntegrityFailure,
  }
}
const demoTopics = [
  ["DBMS", ["SQL", "Normalization", "Transactions"]],
  ["Data Structures", ["Trees", "Graphs", "Sorting"]],
  ["OOP with C++", ["Classes & objects", "Inheritance", "Polymorphism"]],
  ["Operating Systems", ["Processes", "Memory management", "Deadlocks"]],
] as const
const prompts: Record<string, string[]> = {
  SQL: [
    "What is a primary key?",
    "How does an INNER JOIN differ from a LEFT JOIN?",
    "What is the purpose of GROUP BY?",
  ],
  Normalization: [
    "Define first normal form.",
    "Explain functional dependency with an example.",
    "How does BCNF reduce data redundancy?",
  ],
  Transactions: [
    "What does ACID stand for?",
    "Explain conflict serializability.",
    "Compare optimistic and pessimistic concurrency control.",
  ],
  Trees: [
    "What is a binary tree?",
    "Explain inorder traversal.",
    "How does an AVL tree maintain balance?",
  ],
  Graphs: [
    "Define a directed graph.",
    "Compare BFS and DFS.",
    "Explain Dijkstra’s shortest-path algorithm.",
  ],
  Sorting: [
    "What is a stable sort?",
    "Explain merge sort with an example.",
    "Compare the time complexity of quicksort and heapsort.",
  ],
  "Classes & objects": [
    "What is encapsulation?",
    "Explain constructors and destructors.",
    "Compare a class and a struct in C++.",
  ],
  Inheritance: [
    "What is inheritance?",
    "Explain multiple inheritance in C++.",
    "How does virtual inheritance resolve the diamond problem?",
  ],
  Polymorphism: [
    "What is a virtual function?",
    "Compare overloading and overriding.",
    "Explain runtime polymorphism with an example.",
  ],
  Processes: [
    "What is a process?",
    "Compare processes and threads.",
    "Explain round-robin scheduling.",
  ],
  "Memory management": [
    "What is paging?",
    "Explain virtual memory.",
    "Compare FIFO and LRU page replacement.",
  ],
  Deadlocks: [
    "What is a deadlock?",
    "List the necessary conditions for deadlock.",
    "Explain the banker’s algorithm.",
  ],
}
export function mockFile(
  scenario: "bank" | "errors" | "empty" | "server",
): File {
  const rows: string[][] = [columns]
  if (scenario === "bank")
    for (const [subject, topics] of demoTopics)
      for (const topic of topics)
        for (const [marks, count] of [
          [2, 3],
          [5, 2],
          [10, 1],
        ])
          for (let i = 0; i < count; i++)
            rows.push([
              `Q${rows.length}`,
              `${prompts[topic][i % 3]}${
                marks === 10
                  ? " Discuss with a suitable example."
                  : marks === 5
                    ? " Give reasons for your answer."
                    : ""
              }`,
              subject,
              topic,
              "2",
              String(marks),
            ])
  if (scenario === "errors") rows.push(["Q1", "", "DBMS", "SQL", "2", "-2"])
  const csv = rows
    .map((row) =>
      row.map((value) => `"${value.replace(/"/g, '""')}"`).join(","),
    )
    .join("\r\n")
  return new File(
    [csv],
    scenario === "server" ? "demo-server-error.csv" : `demo-${scenario}.csv`,
    { type: "text/csv" },
  )
}
