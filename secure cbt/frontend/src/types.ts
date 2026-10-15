export interface ValidationError {
  row: number
  field: string
  problem: string
}
export interface MarksGroup {
  marks: number
  available: number
}
export interface Topic {
  name: string
  total: number
  marksGroups: MarksGroup[]
}
export interface Subject {
  name: string
  total: number
  topics: Topic[]
}
export interface BankSummary {
  totalQuestions: number
  subjects: Subject[]
}
export interface UploadResult {
  filename: string
  validCount: number
  isValid: boolean
  isEmptyBank: boolean
  canContinue: boolean
  errors: ValidationError[]
  warnings?: string[]
  summary?: BankSummary
}
export interface Selection {
  subject: string
  topic: string
  marks: number
  count: number
}
export interface Question {
  number: number
  id: string
  subject: string
  topic: string
  marks: number
  text: string
}
export interface Paper {
  totalQuestions: number
  totalMarks: number
  questions: Question[]
  paperText: string
}
export interface LockResult {
  originalBits: number
  compressedBits: number
  compressedBytes: number
  encryptedBytes: number
  packageFile: string
  verified: boolean
}
export interface ErrorResponse {
  error: string
  problems?: string[]
}
export type Screen = 0 | 1 | 2 | 3
