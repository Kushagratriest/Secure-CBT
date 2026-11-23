export default function Icon({
  name,
  className = "",
}: {
  name: "shield" | "upload" | "check" | "alert" | "lock" | "arrow" | "file" | "chevron"
  className?: string
}) {
  const paths = {
    shield: "M12 3 5 6v5c0 5 4 8 7 10 3-2 7-5 7-10V6l-7-3Z M9 12l2 2 4-4",
    upload: "M12 16V4m0 0L7 9m5-5 5 5M5 15v5h14v-5",
    check: "m5 12 4 4L19 6",
    alert:
      "M12 8v5m0 4v.1M10.3 4.7a2 2 0 0 1 3.4 0l7.6 13.2a2 2 0 0 1-1.7 3H4.4a2 2 0 0 1-1.7-3L10.3 4.7Z",
    lock: "M7 10V7a5 5 0 0 1 10 0v3M5 10h14v11H5V10Zm7 4v3",
    arrow: "M5 12h14m-5-5 5 5-5 5",
    file: "M14 3H5v18h14V8l-5-5Zm0 0v6h5M9 13h6m-6 4h6",
    chevron: "m8 10 4 4 4-4",
  }
  return (
    <svg
      aria-hidden="true"
      className={`icon ${className}`}
      viewBox="0 0 24 24"
      fill="none"
    >
      <path
        d={paths[name]}
        stroke="currentColor"
        strokeWidth="1.6"
        strokeLinecap="round"
        strokeLinejoin="round"
      />
    </svg>
  )
}
