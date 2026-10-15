import { useRef, useState } from "react"
import Icon from "./Icon"
export default function DropZone({
  disabled,
  onFile,
}: {
  disabled: boolean
  onFile: (file: File) => void
}) {
  const input = useRef<HTMLInputElement>(null)
  const [dragging, setDragging] = useState(false)
  return (
    <div
      className={`drop-zone ${dragging ? "dragging" : ""}`}
      role="button"
      tabIndex={disabled ? -1 : 0}
      aria-label="Choose CSV question bank"
      aria-disabled={disabled}
      onClick={() => !disabled && input.current?.click()}
      onKeyDown={(e) => {
        if (e.key === "Enter" || e.key === " ") {
          e.preventDefault()
          if (!disabled) input.current?.click()
        }
      }}
      onDragOver={(e) => {
        e.preventDefault()
        if (!disabled) setDragging(true)
      }}
      onDragLeave={() => setDragging(false)}
      onDrop={(e) => {
        e.preventDefault()
        setDragging(false)
        if (!disabled && e.dataTransfer.files[0])
          onFile(e.dataTransfer.files[0])
      }}
    >
      <input
        ref={input}
        type="file"
        accept=".csv,text/csv"
        className="sr-only"
        disabled={disabled}
        onChange={(e) => {
          if (e.target.files?.[0]) onFile(e.target.files[0])
          e.target.value = ""
        }}
      />
      <div className="icon-tile">
        <Icon name="upload" />
      </div>
      <h3 className="desktop-copy">Drop your question bank here</h3>
      <h3 className="touch-copy">Tap to choose a CSV file</h3>
      <p className="muted desktop-copy">or click to browse files</p>
      <span className="file-type">CSV question bank</span>
    </div>
  )
}
