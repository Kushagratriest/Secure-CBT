// Secure CBT web server.
// Sits between the JavaScript frontend (in ../frontend) and the C++ phase code.
// The browser sends requests with fetch(); this file calls the existing
// Phase 1 functions and replies with JSON. It also serves the frontend files,
// so everything is opened from one address: http://localhost:8080
//
// Build (from the Secure-CBT folder):
//   clang++ -std=c++17 -Icommon -Iphase1-paper-setting -Iserver server/server.cpp -o secure_cbt_server
// Run (from the Secure-CBT folder):
//   ./secure_cbt_server
//
// Routes:
//   POST /api/upload    body = raw CSV text        -> check result + subject/topic/marks summary
//   POST /api/generate  body = JSON selections     -> the generated paper (preview)
//   POST /api/lock      (no body)                  -> compress + encrypt + save, returns sizes

#include <iostream>
#include <string>
#include <vector>
#include <memory>              // unique_ptr: lets us throw away the old bank and make a fresh one
#include <mutex>               // mutex: stops two requests changing the shared data at the same time
#include <filesystem>          // create folders, find the temp folder
#include <fstream>

#include "httplib.h"           // cpp-httplib: the HTTP server
#include "json.hpp"            // nlohmann/json: build and read JSON

#include "Question.h"
#include "QuestionBank.h"
#include "TopicIndex.h"
#include "PaperGenerator.h"
#include "HuffmanCompressor.h"
#include "KeyManager.h"
#include "csvchecker.h"
#include "PaperStorage.h"

using namespace std;
using json = nlohmann::json;   // shorter name for the JSON type
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Shared state. The server remembers the uploaded bank and the generated paper
// between requests (upload -> generate -> lock). One user at a time (demo).
// ---------------------------------------------------------------------------
struct ServerState {
    unique_ptr<QuestionBank> bank;    // the uploaded questions (nullptr until a valid upload)
    unique_ptr<TopicIndex> index;     // subject -> topic -> marks -> IDs
    string paperText;                 // the last generated paper ("" until /api/generate succeeds)
};

ServerState state;                    // the one shared copy
mutex stateMutex;                     // lock this before reading or changing 'state'

const string OUTPUT_DIR = "output";   // where paper.scbt and paper.key are written
const string FRONTEND_DIR = "frontend/dist";   // the built website (run "pnpm build" inside frontend/ to create it)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Turns a json object into text. Invalid UTF-8 characters (e.g. from a CSV
// saved in an old Excel encoding) are replaced instead of crashing the server.
string toText(const json& j) {
    return j.dump(-1, ' ', false, json::error_handler_t::replace);   // -1 = no pretty spacing
}

// Sends a JSON reply with the given HTTP status code.
void reply(httplib::Response& res, int status, const json& body) {
    res.status = status;                                       // 200 = OK, 400 = bad request, etc.
    res.set_content(toText(body), "application/json");         // tell the browser it's JSON
}

// Sends {"error": "..."} with the given status.
void replyError(httplib::Response& res, int status, const string& message) {
    reply(res, status, json{{"error", message}});
}

// Builds the subject -> topic -> marks summary the configure screen shows.
json buildSummary(QuestionBank& bank, const TopicIndex& index) {
    json subjects = json::array();                             // list of subjects
    for (const string& subject : index.getSubjects()) {        // every subject, A-Z
        json topics = json::array();
        int subjectTotal = 0;                                  // questions in this subject
        for (const string& topic : index.getTopics(subject)) { // every topic in it, A-Z
            json marksGroups = json::array();
            int topicTotal = 0;                                // questions in this topic
            for (int marks : index.getMarkValues(subject, topic)) {   // every mark value, low to high
                int available = index.countQuestions(subject, topic, marks);
                marksGroups.push_back({{"marks", marks}, {"available", available}});
                topicTotal += available;
            }
            topics.push_back({{"name", topic}, {"total", topicTotal}, {"marksGroups", marksGroups}});
            subjectTotal += topicTotal;
        }
        subjects.push_back({{"name", subject}, {"total", subjectTotal}, {"topics", topics}});
    }
    return json{{"totalQuestions", bank.size()}, {"subjects", subjects}};
}

// ---------------------------------------------------------------------------
// POST /api/upload
// Body: the CSV file's text. Optional header "X-Filename" carries the file name.
// ---------------------------------------------------------------------------
void handleUpload(const httplib::Request& req, httplib::Response& res) {
    if (req.body.empty()) {                                    // nothing was sent
        replyError(res, 400, "No file content received");
        return;
    }

    // checkCsv() reads from a file path, so write the upload to a temp file first.
    fs::path tempPath = fs::temp_directory_path() / "secure_cbt_upload.csv";
    {
        ofstream out(tempPath, ios::binary);                   // binary: keep bytes exactly (BOM, \r\n)
        if (!out.is_open()) {
            replyError(res, 500, "Server could not store the upload");
            return;
        }
        out.write(req.body.data(), req.body.size());           // the CSV text as sent by the browser
    }

    CheckResult check = checkCsv(tempPath.string());           // the same checker the terminal version uses
    fs::remove(tempPath);                                      // the temp file is no longer needed

    string filename = req.get_header_value("X-Filename");      // the real file name, if the browser sent it
    if (filename.empty()) filename = "upload.csv";
    check.filename = filename;

    json body;
    body["filename"] = check.filename;
    body["validCount"] = check.validCount;
    body["isValid"] = check.isValid();
    body["isEmptyBank"] = check.isEmptyBank();
    body["canContinue"] = check.canContinue();
    body["errors"] = json::array();
    for (const RowError& e : check.errors) {                   // every problem, for the error table
        body["errors"].push_back({{"row", e.row}, {"field", e.field}, {"problem", e.problem}});
    }

    lock_guard<mutex> lock(stateMutex);                        // we are about to change shared state
    state.paperText = "";                                      // a new upload invalidates any old paper

    if (!check.canContinue()) {                                // errors, or no questions
        state.bank.reset();                                    // forget any previous bank
        state.index.reset();
        reply(res, 200, body);                                 // 200: the check itself worked, the file has problems
        return;
    }

    state.bank = make_unique<QuestionBank>();                  // fresh, empty bank
    state.index = make_unique<TopicIndex>();                   // fresh, empty index
    body["warnings"] = json::array();

    for (const Question& q : check.questions) {
        if (state.bank->hasQuestion(q.getId())) {              // duplicate ID: keep the first one
            body["warnings"].push_back("Duplicate question_id \"" + q.getId() + "\" skipped (first one kept)");
            continue;
        }
        state.bank->addQuestion(q);
        state.index->addToIndex(q.getSubject(), q.getTopic(), q.getMarks(), q.getId());
    }

    body["summary"] = buildSummary(*state.bank, *state.index); // what the configure screen draws
    reply(res, 200, body);
}

// ---------------------------------------------------------------------------
// POST /api/generate
// Body: {"selections":[{"subject":"DBMS","topic":"SQL","marks":2,"count":3}, ...]}
// ---------------------------------------------------------------------------
void handleGenerate(const httplib::Request& req, httplib::Response& res) {
    json input;
    try {
        input = json::parse(req.body);                         // turn the text into a json object
    } catch (const json::parse_error&) {                       // not valid JSON
        replyError(res, 400, "Request body is not valid JSON");
        return;
    }
    if (!input.contains("selections") || !input["selections"].is_array()) {
        replyError(res, 400, "Expected {\"selections\": [...]}");
        return;
    }

    // Turn every selection into a SelectionRequest, checking the types.
    vector<SelectionRequest> requests;
    for (const json& s : input["selections"]) {
        if (!s.is_object() || !s.contains("subject") || !s.contains("topic")
            || !s.contains("marks") || !s.contains("count")
            || !s["subject"].is_string() || !s["topic"].is_string()
            || !s["marks"].is_number_integer() || !s["count"].is_number_integer()) {
            replyError(res, 400, "Each selection needs subject (text), topic (text), marks (whole number), count (whole number)");
            return;
        }
        int count = s["count"].get<int>();
        if (count == 0) continue;                              // 0 means "none from this group": skip it
        requests.push_back({s["subject"].get<string>(), s["topic"].get<string>(), s["marks"].get<int>(), count});
    }
    if (requests.empty()) {
        replyError(res, 400, "No questions selected");
        return;
    }

    lock_guard<mutex> lock(stateMutex);
    if (!state.bank || !state.index) {                         // generate called before a valid upload
        replyError(res, 409, "Upload a valid question bank first");
        return;
    }

    PaperGenerator pGen;
    vector<string> problems;
    vector<string> selectedIds = pGen.generateFromSelection(*state.index, requests, problems);
    if (!problems.empty()) {                                   // asked for more than available, negative counts, etc.
        reply(res, 400, json{{"error", "Some selections are not possible"}, {"problems", problems}});
        return;
    }

    // Assemble the paper. Same text format as the terminal version (main.cpp).
    string paperData = "";
    int totalMarks = 0;
    int number = 1;
    json questions = json::array();                            // structured copy for the preview screen

    for (const string& id : selectedIds) {
        const Question* q = state.bank->getQuestion(id);
        if (q == nullptr) continue;                            // cannot happen: every indexed ID is in the bank
        paperData += to_string(number) + ". [" + q->getId() + "] "
                   + "(" + q->getSubject() + " / " + q->getTopic() + ", "
                   + to_string(q->getMarks()) + " marks)\n"
                   + q->getText() + "\n\n";
        questions.push_back({{"number", number}, {"id", q->getId()}, {"subject", q->getSubject()},
                             {"topic", q->getTopic()}, {"marks", q->getMarks()}, {"text", q->getText()}});
        totalMarks += q->getMarks();
        number++;
    }
    paperData += "Total marks: " + to_string(totalMarks) + "\n";

    state.paperText = paperData;                               // remember it for /api/lock

    reply(res, 200, json{{"totalQuestions", (int)questions.size()}, {"totalMarks", totalMarks},
                         {"questions", questions}, {"paperText", paperData}});
}

// ---------------------------------------------------------------------------
// POST /api/lock
// Compresses and encrypts the last generated paper and saves it to output/.
// The key is written to its own file and is NEVER sent to the browser.
// ---------------------------------------------------------------------------
void handleLock(const httplib::Request&, httplib::Response& res) {
    lock_guard<mutex> lock(stateMutex);
    if (state.paperText.empty()) {                             // nothing generated yet
        replyError(res, 409, "Generate a paper first");
        return;
    }

    HuffmanCompressor compressor;
    string compressed = compressor.compress(state.paperText);  // compress the whole paper

    KeyManager keyMgr;
    string rootKey = keyMgr.generateRootKey();
    string encrypted = keyMgr.encrypt(compressed, rootKey);    // XOR for now (AES-256 for the final)

    PaperPackage pkg;
    pkg.bitCount = compressor.getBitCount();
    pkg.codes = compressor.getCodes();
    pkg.encrypted = encrypted;

    error_code ec;
    fs::create_directories(OUTPUT_DIR, ec);                    // make output/ if it doesn't exist
    string packagePath = OUTPUT_DIR + "/paper.scbt";
    string keyPath = OUTPUT_DIR + "/paper.key";
    string error;

    if (!savePaperPackage(packagePath, pkg, error) || !saveKeyFile(keyPath, rootKey, error)) {
        replyError(res, 500, "Could not save the paper: " + error);
        return;
    }

    PaperPackage check;                                        // read it back to prove it was saved correctly
    bool verified = loadPaperPackage(packagePath, check, error)
                 && check.bitCount == pkg.bitCount
                 && check.codes == pkg.codes
                 && check.encrypted == pkg.encrypted;

    reply(res, 200, json{
        {"originalBits", (int)state.paperText.size() * 8},
        {"compressedBits", pkg.bitCount},
        {"compressedBytes", (int)compressed.size()},
        {"encryptedBytes", (int)encrypted.size()},
        {"packageFile", packagePath},
        {"verified", verified}
    });
}

// ---------------------------------------------------------------------------
// main: set up routes and start listening
// ---------------------------------------------------------------------------
int main() {
    srand(time(0));                                            // random seed for question picking

    httplib::Server svr;
    svr.set_payload_max_length(5 * 1024 * 1024);               // refuse uploads over 5 MB

    if (!svr.set_mount_point("/", FRONTEND_DIR)) {             // serve index.html, css, js
        cout << "Note: '" << FRONTEND_DIR << "' folder not found; API still works, no web pages served." << endl;
    }

    svr.Post("/api/upload", handleUpload);
    svr.Post("/api/generate", handleGenerate);
    svr.Post("/api/lock", handleLock);

    // Any exception inside a handler becomes a 500 reply instead of killing the server.
    svr.set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr ep) {
        string msg = "Internal server error";
        try { if (ep) rethrow_exception(ep); }
        catch (const exception& e) { msg += string(": ") + e.what(); }
        catch (...) {}
        replyError(res, 500, msg);
    });

    const char* host = "127.0.0.1";                            // this computer only: not reachable from the network
    int port = 8080;
    cout << "Secure CBT server running at http://localhost:" << port << endl;
    if (!svr.listen(host, port)) {                             // blocks here until the server stops
        cout << "Could not start the server (is port " << port << " already in use?)" << endl;
        return 1;
    }
    return 0;
}
