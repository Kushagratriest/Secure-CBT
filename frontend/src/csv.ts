export const columns = ["question_id", "question_text", "subject", "topic", "difficulty", "marks"];

export type CSVRecord = { row: number; values: string[] };

export function hasValidHeaders(values: string[] | undefined) {
  return values?.length === columns.length && columns.every((column, index) => values[index] === column);
}

export function parseCSV(content: string): CSVRecord[] {
  const records: CSVRecord[] = [];
  let values: string[] = [];
  let value = "";
  let quoted = false;
  let closedQuote = false;
  let line = 1;
  let startLine = 1;
  let hasCSVContent = false;
  const finishRecord = () => {
    values.push(value.trim());
    if (hasCSVContent || values.some(Boolean)) records.push({ row: startLine, values });
    values = [];
    value = "";
    closedQuote = false;
    hasCSVContent = false;
  };
  const source = content.replace(/^\uFEFF/, "");
  if (source.includes("\0")) throw new Error("Invalid text encoding. Save your CSV as UTF-8.");
  for (let index = 0; index < source.length; index++) {
    const character = source[index];
    if (quoted) {
      if (character === '"') {
        if (source[index + 1] === '"') {
          value += '"';
          index++;
        } else {
          quoted = false;
          closedQuote = true;
        }
      } else {
        value += character;
        if (character === "\n" || (character === "\r" && source[index + 1] !== "\n")) line++;
      }
    } else if (character === ",") {
      hasCSVContent = true;
      values.push(value.trim());
      value = "";
      closedQuote = false;
    } else if (character === "\r" || character === "\n") {
      finishRecord();
      if (character === "\r" && source[index + 1] === "\n") index++;
      line++;
      startLine = line;
    } else if (character === '"' && !value && !closedQuote) {
      hasCSVContent = true;
      quoted = true;
    } else {
      if (closedQuote || character === '"') throw new Error(`Row ${line}: unexpected text or quote. Check CSV formatting.`);
      value += character;
    }
  }
  if (quoted) throw new Error(`Row ${startLine}: missing closing quote.`);
  finishRecord();
  return records;
}

export function createTemplate() {
  const rows = [
    columns,
    ["Q1", "The number of players who can play chess are 2, tell me this this this", "Chess", "Basics", "2", "4"],
    ["Q2", "What is a binary tree?", "DSA", "Trees", "3", "5"],
  ];
  return rows.map((row) => row.map((value) => `"${value.replace(/"/g, '""')}"`).join(",")).join("\r\n") + "\r\n";
}
