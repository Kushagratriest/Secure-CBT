#include <iostream>
#include <string>
#include <vector>
#include "Question.h"
#include "QuestionBank.h"
#include "TopicIndex.h"
#include "PaperGenerator.h"
#include "HuffmanCompressor.h"
#include "KeyManager.h"
#include "csvchecker.h"        // checkCsv(): loads and validates the question CSV
#include "PaperStorage.h"      // saves the encrypted paper + key to files
using namespace std;

// Usage: ./secure_cbt [path-to-questions.csv]
// If no path is given, "questions.csv" in the current folder is used.
int main(int argc, char* argv[]) {
    cout << "===== PHASE 1: PAPER SETTING =====" << endl;

    string csvPath = (argc > 1) ? argv[1] : "questions.csv";        // use the path typed after the program name, or the default

    // step 1 - load and check questions from the CSV
    CheckResult check = checkCsv(csvPath);                          // read + validate every row
    cout << "\nChecked file: " << check.filename << endl;

    if (!check.isValid()) {                                         // the file has at least one problem
        cout << "The question file has " << check.errors.size() << " problem(s):" << endl;
        for (const RowError& e : check.errors) {                    // print every problem so the teacher can fix them all
            if (e.row == 0) cout << "  [File] " << e.problem << endl;                                  // whole-file problem
            else            cout << "  Row " << e.row << " [" << e.field << "] " << e.problem << endl; // one row's problem
        }
        cout << "Fix the file and run again. Paper NOT generated." << endl;
        return 1;                                                   // non-zero = the program failed
    }
    if (check.isEmptyBank()) {                                      // header is fine but there are no questions
        cout << "No questions found in the file. Paper NOT generated." << endl;
        return 1;
    }
    cout << "Valid questions in file: " << check.validCount << endl;

    // step 2 - add to bank and index
    QuestionBank qBank;
    TopicIndex tIndex;

    for (const Question& q : check.questions) {
        if (qBank.hasQuestion(q.getId())) {                         // same ID appeared earlier in the file
            cout << "  Warning: duplicate question_id \"" << q.getId() << "\" skipped (first one kept)" << endl;
            continue;                                               // skip it so the index doesn't list the ID twice
        }
        qBank.addQuestion(q);                                       // store the question by its ID
        tIndex.addToIndex(q.getSubject(), q.getTopic(), q.getMarks(), q.getId());   // file it under subject -> topic -> marks
    }

    // step 3 - "configure paper" screen (console version for now)
    // Shows the bank grouped as Subject -> Topic -> marks, and asks how many
    // questions the user wants from each marks group. The total adds up live.
    cout << "\n===== CONFIGURE PAPER =====" << endl;
    cout << "Total questions in bank: " << qBank.size() << endl;

    vector<SelectionRequest> requests;                              // the user's choices, one per group
    int totalQuestions = 0;                                         // running total of questions chosen
    int totalMarksChosen = 0;                                       // running total of marks chosen

    for (const string& subject : tIndex.getSubjects()) {            // every subject, A-Z
        cout << "\nSubject: " << subject << endl;
        for (const string& topic : tIndex.getTopics(subject)) {     // every topic in this subject, A-Z
            cout << "  Topic: " << topic << endl;
            for (int marks : tIndex.getMarkValues(subject, topic)) {    // every mark value in this topic, low to high
                int available = tIndex.countQuestions(subject, topic, marks);   // how many questions exist here
                int wanted = -1;                                    // -1 = no valid answer yet
                while (wanted < 0) {                                // keep asking until the answer is valid
                    cout << "    " << marks << "-mark questions (" << available << " available). How many? ";
                    string answer;
                    if (!getline(cin, answer)) { wanted = 0; break; }   // input ended (e.g. piped file ran out): treat as 0
                    answer = trim(answer);                          // ignore spaces around the number
                    if (answer.empty()) { wanted = 0; break; }      // just pressing Enter means 0
                    int n = 0;
                    if (!parsePositiveInt(answer, n)) {             // not a whole number (e.g. "abc", "-1", "2.5")
                        cout << "      Please type a whole number from 0 to " << available << "." << endl;
                    } else if (n > available) {                     // more than exist in this group
                        cout << "      Only " << available << " available here." << endl;
                    } else {
                        wanted = n;                                 // valid answer
                    }
                }
                if (wanted > 0) {                                   // only remember groups the user actually wants
                    requests.push_back({subject, topic, marks, wanted});
                    totalQuestions += wanted;                       // update the live totals
                    totalMarksChosen += wanted * marks;
                }
                cout << "      Running total: " << totalQuestions << " questions, " << totalMarksChosen << " marks" << endl;
            }
        }
    }

    if (totalQuestions == 0) {                                      // user picked nothing at all
        cout << "\nNo questions selected. Paper NOT generated." << endl;
        return 1;
    }
    cout << "\nPaper will have " << totalQuestions << " questions, " << totalMarksChosen << " marks." << endl;

    // step 4 - generate paper from the user's choices
    PaperGenerator pGen;
    vector<string> problems;                                        // filled if any request is impossible
    vector<string> selectedIds = pGen.generateFromSelection(tIndex, requests, problems);
    if (!problems.empty()) {                                        // shouldn't happen (inputs were capped above), but never build a broken paper
        cout << "Could not generate the paper:" << endl;
        for (const string& p : problems) cout << "  " << p << endl;
        return 1;
    }

    cout << "\nSelected questions for paper:" << endl;
    for (string id : selectedIds) {
        cout << "  " << id << endl;
    }

    // step 5 - assemble the full paper text, then compress the whole thing
    string paperData = "";                                          // the complete paper as one string
    int totalMarks = 0;                                             // sum of marks of all selected questions
    int number = 1;                                                 // question number printed on the paper (1, 2, 3...)

    for (string id : selectedIds) {
        const Question* q = qBank.getQuestion(id);                  // look up the full question by its ID
        if (q == nullptr) {                                         // should never happen: every indexed ID was added to the bank
            cout << "  Warning: " << id << " not found in bank, skipped" << endl;
            continue;
        }
        paperData += to_string(number) + ". [" + q->getId() + "] "              // "1. [Q8] "
                   + "(" + q->getSubject() + " / " + q->getTopic() + ", "      // "(CS / OOP, "
                   + to_string(q->getMarks()) + " marks)\n"                     // "4 marks)" + new line
                   + q->getText() + "\n\n";                                    // the question itself + blank line
        totalMarks += q->getMarks();                                // add this question's marks to the total
        number++;                                                   // next question number
    }
    paperData += "Total marks: " + to_string(totalMarks) + "\n";    // footer line with the paper total

    cout << "\n----- Assembled paper (" << paperData.size() << " characters) -----\n"
         << paperData
         << "------------------------------------------" << endl;

    HuffmanCompressor compressor;
    string compressed = compressor.compress(paperData);             // compress the WHOLE paper text, not just IDs

    // step 6 - encrypt
    KeyManager keyMgr;
    string rootKey = keyMgr.generateRootKey();
    string encrypted = keyMgr.encrypt(compressed, rootKey);         // encrypt the compressed bytes

    cout << "\nEncrypted paper size: " << encrypted.size() << " bytes" << endl;
    cout << "Paper encrypted successfully" << endl;

    // step 7 - save the encrypted paper for Phase 2
    PaperPackage pkg;                                               // bundle everything Phase 3 needs (except the key)
    pkg.bitCount = compressor.getBitCount();                        // real bits, so padding can be ignored later
    pkg.codes = compressor.getCodes();                              // Huffman table, needed to decompress later
    pkg.encrypted = encrypted;                                      // the encrypted bytes themselves

    const string packagePath = "paper.scbt";                        // the encrypted paper file
    const string keyPath = "paper.key";                             // the key, kept in a SEPARATE file
    string error;                                                   // filled with a reason if saving fails

    if (!savePaperPackage(packagePath, pkg, error)) {               // write the package
        cout << "Could not save the paper: " << error << endl;
        return 1;
    }
    if (!saveKeyFile(keyPath, rootKey, error)) {                    // write the key (demo only)
        cout << "Could not save the key: " << error << endl;
        return 1;
    }

    // read the file straight back and compare, to prove it was saved correctly
    PaperPackage check2;                                            // empty package to load into
    if (!loadPaperPackage(packagePath, check2, error)) {
        cout << "Saved file could not be read back: " << error << endl;
        return 1;
    }
    bool same = check2.bitCount == pkg.bitCount                     // same bit count
             && check2.codes == pkg.codes                           // same code table
             && check2.encrypted == pkg.encrypted;                  // same encrypted bytes
    if (!same) {
        cout << "Saved file does not match what was written." << endl;
        return 1;
    }

    cout << "Saved encrypted paper to " << packagePath << " (verified by reading it back)" << endl;
    cout << "Saved root key to " << keyPath << " (demo only: in the real system the key stays on the server)" << endl;
    cout << "===== PHASE 1 COMPLETE =====" << endl;

    return 0;
}