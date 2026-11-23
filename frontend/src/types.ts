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
  unlockTimestamp?: number
  unlockTimeFormatted?: string
}
export interface ErrorResponse {
  error: string
  problems?: string[]
}
export type Portal = "board" | "centre" | "student"
export type Screen = 0 | 1 | 2 | 3 | 4

// Phase 2: Distribution Network, AVL Registry, BFS Hops & SHA-256 Audit Log
export interface DistNode {
  nodeId: string
  name: string
  role: string
  parentId: string
  status: "Registered" | "Pending" | "Rejected"
  keyFingerprint: string
  delivered: boolean
  payloadBytes: number
}

export interface HopReport {
  hopNumber: number
  fromId: string
  toId: string
  status: string
  payloadBytes: number
}

export interface AuditEntry {
  index: number
  fromNode: string
  toNode: string
  timestamp: string
  prevHash: string
  hash: string
}

export interface AuditState {
  intact: boolean
  brokenAt: number
  tamperedIndex: number
  entries: AuditEntry[]
}

// Live exam status and fullscreen proctor alert count synced from each student terminal
export interface StudentAttemptStatus {
  centreId: string
  studentRoll: string
  status: "Forwarded" | "Attempting" | "Submitted"
  answeredCount: number
  reviewCount: number
  totalQuestions: number
  exitWarnings: number
  timeRemainingSeconds: number
  autoSubmitted: boolean
  updatedAt: string
}

export interface NetworkState {
  unlockTimestamp?: number
  unlockTimeFormatted?: string
  totalNodes: number
  avlHeight: number
  isBalanced: boolean
  avlRootId: string
  hasPackage: boolean
  packageEncryptedBytes: number
  packageCompressedBits: number
  broadcastDone: boolean
  nodes: DistNode[]
  hops: HopReport[]
  audit: AuditState
  // Map of centreId -> allocated exam countdown seconds
  centreDurations?: Record<string, number>
  // Live tracking list for all forwarded and attempting students
  studentAttempts?: StudentAttemptStatus[]
}

export interface CentreForwardReceipt {
  studentNodeId: string
  studentRoll: string
  centreId: string
  forwarded: boolean
  payloadBytes: number
  keyFingerprint: string
}

export interface CentreForwardResult {
  centreId: string
  examDurationSeconds?: number
  receipts: CentreForwardReceipt[]
  network: NetworkState
}

export interface StudentPortalResponse {
  forwarded: boolean
  locked: boolean
  centreId: string
  studentRoll: string
  studentNodeId?: string
  // Centre-allocated exam duration in seconds
  examDurationSeconds?: number
  unlockTimestamp: number
  unlockTimeFormatted: string
  currentClockFormatted: string
  message?: string
  encryptedBytes?: number
  onionLayers?: number
  keyFingerprint?: string
  secondsRemaining?: number
  layersPeeled?: number
  compressedBytes?: number
  plainBytes?: number
  shuffleSeed?: number
  questionCount?: number
  shuffledQuestions?: string[]
  formattedPaper?: string
}

// Phase 3: Exam Centre Time-Lock, Student Shuffle & Leak Traceability
export interface ExamSlotInfo {
  slotId: string
  centreId: string
  examName: string
  unlockTimeFormatted: string
  unlockedAtClock: boolean
  secondsRemaining: number
}

export interface CentreUnlockResult {
  unlocked: boolean
  centreId: string
  studentId: string
  shuffleSeed?: number
  simulateEarly: boolean
  currentClockFormatted: string
  targetUnlockFormatted: string
  blockedReason?: string
  layersPeeled?: number
  encryptedBytes: number
  compressedBytes?: number
  plainBytes?: number
  questionCount?: number
  shuffledQuestions?: string[]
  formattedPaper?: string
  slots: ExamSlotInfo[]
}

export interface LeakTraceReport {
  traced: boolean
  leakedSha256: string
  leakedBytes: number
  culpritNodeId: string
  culpritName: string
  culpritRole: string
  statusInRegistry: string
  keyFingerprint: string
  hierarchyDepth: number
  chainOfCustody: string[]
}
