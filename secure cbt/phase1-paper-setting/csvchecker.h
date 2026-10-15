// Checks an uploaded question-bank CSV and reports what it found.

#pragma once                 
#include <string>             
#include <vector>            
#include <fstream>           // ifstream, to open and read the CSV file
#include <cctype>            // isdigit, isspace
#include "Question.h"        // valid rows are turned into Question objects
using namespace std;         

// finds the errors in the file.
struct RowError {
    int row;                 // Which row has the problem. 0 means the whole file (e.g. bad header).
    string field;            // Which column is wrong, e.g. "difficulty". "File" for file-level errors.
    string problem;          // Plain-language message shown to the teacher.
};

// Everything the checker reports back about one upload.
struct CheckResult {
    // The name of the uploaded file, e.g. "secure-cbt-question-template.csv".
    string filename;

    // How many questions passed every check.
    // A header-only file ends with 0.
    int validCount = 0;

    // Every problem found, in the order the checker found them.
    // An empty list means the file has no errors.
    vector<RowError> errors;

    // Every row that passed all checks, converted to a Question.
    // validCount always equals questions.size().
    vector<Question> questions;

    // Returns true when the file has no errors.
    bool isValid() const {
        // empty() returns true when the list has zero items.
        return errors.empty();
    }

    // Returns true when the file is well formed but has no questions.
    // The UI uses this to show the "No questions found" screen
    bool isEmptyBank() const {
        // Both conditions must hold: no errors, and zero valid questions.
        return isValid() && validCount == 0;
    }

    // Returns true when the file can move on to the next step.
    bool canContinue() const {
        return isValid() && validCount > 0;
    }
};


// ---------------------------------------------------------------------------
// Helpers used by checkCsv. They are "inline" because this is a header:
// if two .cpp files include it, non-inline functions would be defined twice
// and the linker would fail.
// ---------------------------------------------------------------------------

// The exact header the template must start with.
const string EXPECTED_CSV_HEADER = "question_id,question_text,subject,topic,difficulty,marks";
const int EXPECTED_CSV_COLUMNS = 6;   // number of columns every row must have

// Removes a trailing '\r'. Files saved on Windows/Excel end lines with "\r\n",
// and getline only strips the '\n', so without this every last field and the
// header comparison would fail.
inline void stripCarriageReturn(string& line) {
    if (!line.empty() && line.back() == '\r') {   // is the last character a carriage return?
        line.pop_back();                            // yes: delete that one character
    }
}

// Removes spaces/tabs at both ends, so " DSA " counts as "DSA" and "   "
// counts as empty.
inline string trim(const string& s) {
    size_t start = 0;                                                       // index of first kept character
    while (start < s.size() && isspace((unsigned char)s[start])) start++;   // skip spaces at the front
    size_t end = s.size();                                                  // one past the last kept character
    while (end > start && isspace((unsigned char)s[end - 1])) end--;        // skip spaces at the back
    return s.substr(start, end - start);                                    // copy out the middle part
}

// Splits one CSV line into fields.
// A comma inside double quotes is part of the text, not a separator.
// Inside quotes, two double quotes ("") mean one literal quote character.
// Sets unclosedQuote to true if the line ends while still inside quotes.
inline vector<string> splitCsvLine(const string& line, bool& unclosedQuote) {
    vector<string> fields;            // the finished fields go here
    string current = "";              // the field we are building right now
    bool inQuotes = false;            // are we currently inside "..."?

    for (size_t i = 0; i < line.size(); i++) {   // look at every character one by one
        char c = line[i];                        // the current character
        if (inQuotes) {                          // --- we are inside quotes ---
            if (c == '"') {                      // found a quote character
                if (i + 1 < line.size() && line[i + 1] == '"') {   // is the next one also a quote?
                    current += '"';              // yes: "" means one literal quote in the text
                    i++;                         // skip the second quote so it isn't read again
                } else {
                    inQuotes = false;            // no: this quote closes the quoted section
                }
            } else {
                current += c;                    // any other char (including commas) is plain text
            }
        } else {                                 // --- we are outside quotes ---
            if (c == '"') {
                inQuotes = true;                 // a quote here opens a quoted section
            } else if (c == ',') {
                fields.push_back(current);       // a comma ends the current field: save it
                current = "";                    // start a new empty field
            } else {
                current += c;                    // normal character: add it to the field
            }
        }
    }
    fields.push_back(current);        // save the last field (no comma comes after it)
    unclosedQuote = inQuotes;         // still inside quotes at the end = quote never closed
    return fields;                    // hand back all fields
}

// Parses a whole-number string strictly: digits only, no sign, no decimals,
// no letters. Rejects anything longer than 9 digits so stoi can't overflow.
// Returns true and fills 'out' on success.
inline bool parsePositiveInt(const string& s, int& out) {
    if (s.empty() || s.size() > 9) return false;      // empty, or too big to fit in an int safely
    for (char c : s) {                                // check every character
        if (!isdigit((unsigned char)c)) return false; // anything not 0-9 (like '-', '.', 'a') fails
    }
    out = stoi(s);                                    // safe now: convert text to a number
    return true;                                      // success
}

// Gets "file.csv" from "some/folder/file.csv" (handles / and \).
inline string baseName(const string& path) {
    size_t slash = path.find_last_of("/\\");          // position of the last / or \ in the path
    if (slash == string::npos) return path;           // no slash at all: path is already just a name
    return path.substr(slash + 1);                    // everything after the last slash
}

// ---------------------------------------------------------------------------
// checkCsv: opens the CSV at 'path', checks the header and every row, and
// reports what it found. It never throws for bad data; every problem goes
// into result.errors.
// ---------------------------------------------------------------------------
inline CheckResult checkCsv(const string& path) {
    CheckResult result;                       // empty report: 0 valid, no errors, no questions
    result.filename = baseName(path);         // remember which file this report is about

    // --- 1. Open the file ---
    ifstream file(path);                      // try to open the file for reading
    if (!file.is_open()) {                    // could not open (missing file, no permission...)
        result.errors.push_back({0, "File", "Could not open the file. Check that it exists and is readable."});
        return result;                        // nothing else can be checked
    }

    // --- 2. Check the header ---
    string header;                            // will hold the first line
    if (!getline(file, header)) {             // read line 1; fails if the file has no lines at all
        result.errors.push_back({0, "File", "The file is empty. Expected header: " + EXPECTED_CSV_HEADER});
        return result;                        // no header means nothing to check
    }
    stripCarriageReturn(header);              // remove a Windows '\r' at the end of the header
    // Excel's "CSV UTF-8" adds 3 invisible bytes (a BOM) at the start.
    // They are not part of the text, so remove them before comparing.
    if (header.size() >= 3 && header.compare(0, 3, "\xEF\xBB\xBF") == 0) {   // do the first 3 bytes equal the BOM?
        header.erase(0, 3);                   // yes: delete those 3 bytes
    }
    if (header != EXPECTED_CSV_HEADER) {      // exact, case-sensitive comparison with the template
        result.errors.push_back({0, "File", "Header does not match the template. Expected: " + EXPECTED_CSV_HEADER});
        return result;                        // wrong template: stop immediately
    }

    // --- 3. Check every data row ---
    string line;                              // holds one row at a time
    int rowNumber = 0;                        // 1 = first line after the header
    while (getline(file, line)) {             // keep reading until the end of the file
        rowNumber++;                          // counted before skipping blanks, so numbers match the file
        stripCarriageReturn(line);            // remove a Windows '\r' at the end of the row

        if (trim(line).empty()) continue;     // blank line (e.g. trailing newline): not a question, skip it

        bool unclosedQuote = false;                         // splitCsvLine sets this for us
        vector<string> f = splitCsvLine(line, unclosedQuote); // break the row into its fields

        if (unclosedQuote) {                  // a " was opened but never closed: the row can't be trusted
            result.errors.push_back({rowNumber, "Row", "A quote (\") was opened but never closed."});
            continue;                         // move on to the next row
        }
        if ((int)f.size() != EXPECTED_CSV_COLUMNS) {        // too many or too few columns
            result.errors.push_back({rowNumber, "Row",
                "Expected " + to_string(EXPECTED_CSV_COLUMNS) + " columns but found " + to_string(f.size()) + "."});
            continue;                         // can't tell which value is which, so skip the row
        }
        for (string& s : f) s = trim(s);      // remove spaces around every field

        // Check every field and report each one that fails, so the teacher
        // can fix the whole row in one go.
        bool rowOk = true;                    // becomes false as soon as any field fails
        if (f[0].empty()) { result.errors.push_back({rowNumber, "question_id",   "question_id is empty."});   rowOk = false; }
        if (f[1].empty()) { result.errors.push_back({rowNumber, "question_text", "question_text is empty."}); rowOk = false; }
        if (f[2].empty()) { result.errors.push_back({rowNumber, "subject",       "subject is empty."});       rowOk = false; }
        if (f[3].empty()) { result.errors.push_back({rowNumber, "topic",         "topic is empty."});         rowOk = false; }

        int difficulty = 0;                   // filled in by parsePositiveInt
        if (!parsePositiveInt(f[4], difficulty) || difficulty < 1 || difficulty > 5) {   // not a number, or outside 1-5
            result.errors.push_back({rowNumber, "difficulty", "difficulty must be a whole number from 1 to 5 (got \"" + f[4] + "\")."});
            rowOk = false;
        }

        int marks = 0;                        // filled in by parsePositiveInt
        if (!parsePositiveInt(f[5], marks) || marks <= 0) {   // not a number, or zero
            result.errors.push_back({rowNumber, "marks", "marks must be a whole number greater than 0 (got \"" + f[5] + "\")."});
            rowOk = false;
        }

        if (rowOk) {                          // every check passed
            // Question(id, text, topic, difficulty, marks, subject)
            result.questions.push_back(Question(f[0], f[1], f[3], difficulty, marks, f[2]));
            result.validCount++;              // one more good question
        }
    }

    return result;                            // the full report: errors + valid questions
}
