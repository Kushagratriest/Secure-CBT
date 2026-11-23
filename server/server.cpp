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
#include <fstream>             // file streams for reading/writing question banks
#include <unordered_map>       // hash maps for centre durations and student attempt tracking
#include <algorithm>           // std::max and std::min for clamping duration values

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
#include "AuditLog.h"
#include "AVLRegistry.h"
#include "LCRSTree.h"
#include "IndiaNetwork.h"
#include "TimeLock.h"
#include "ExamCentre.h"
#include "LeakTracer.h"

using namespace std;
using json = nlohmann::json;   // shorter name for the JSON type
namespace fs = std::filesystem;

// Live attempt and proctoring state reported by each student terminal.
struct StudentAttemptStatus {
    string centreId;              // exam centre ID, e.g. "CTR-UK-01"
    string studentRoll;           // student roll number, e.g. "2510012263"
    string status;                // "Forwarded", "Attempting", or "Submitted"
    int answeredCount = 0;        // number of questions answered so far
    int reviewCount = 0;          // number of questions marked for review
    int totalQuestions = 0;       // total questions in paper
    int exitWarnings = 0;         // fullscreen exit count detected by proctor lock
    int timeRemainingSeconds = 0; // seconds left on student's exam duration timer
    bool autoSubmitted = false;   // true if auto-submitted when timer reached 00:00
    string updatedAt;             // formatted server timestamp of last update
};

// ---------------------------------------------------------------------------
// Shared state. The server remembers the uploaded bank, generated paper,
// and Phase 2/3 distribution & exam centre state between requests.
// ---------------------------------------------------------------------------
struct ServerState {
    unique_ptr<QuestionBank> bank;    // the uploaded questions (nullptr until a valid upload)
    unique_ptr<TopicIndex> index;     // subject -> topic -> marks -> IDs
    string paperText;                 // the last generated paper ("" until /api/generate succeeds)

    // Phase 2 & Phase 3 state
    PaperPackage lockedPkg;           // last locked paper package (or loaded from disk)
    string rootKey;                   // 32-byte Board root key (stays on server only!)
    unique_ptr<AVLRegistry> registry; // self-balancing AVL tree of authorized nodes
    unique_ptr<LCRSTree> tree;        // Left-Child Right-Sibling hierarchy tree
    unique_ptr<AuditLog> auditLog;    // SHA-256 hash-chained transfer log
    vector<HopReport> hops;           // results of the latest BFS broadcast
    bool broadcastDone = false;       // true after Phase 2 bfsBroadcast succeeds
    time_t scheduledUnlockTime = 0;   // scheduled exam unlock time (Unix seconds)
    int tamperedIndex = -1;           // index modified during tamper demo (-1 if untouched)
    string originalToNode;            // stores real recipient ID so restore can undo tamper
    unordered_map<string, int> centreDurationSeconds;            // centreId -> allocated exam duration in seconds
    unordered_map<string, StudentAttemptStatus> studentAttempts; // "centreId:roll" -> live candidate status
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
void handleLock(const httplib::Request& req, httplib::Response& res) {
    lock_guard<mutex> lock(stateMutex);
    if (state.paperText.empty()) {                             // nothing generated yet
        replyError(res, 409, "Generate a paper first");
        return;
    }

    time_t unlockTs = time(nullptr) + 1800;                    // default: 30 minutes from now
    if (!req.body.empty()) {
        json body = json::parse(req.body, nullptr, false);     // parse optional JSON body
        if (!body.is_discarded() && body.contains("unlockTimestamp") && body["unlockTimestamp"].is_number_integer()) {
            int64_t reqTs = body["unlockTimestamp"].get<int64_t>();
            if (reqTs > 0) unlockTs = (time_t)reqTs;           // use caller-provided timestamp if positive
        }
    }

    HuffmanCompressor compressor;
    string compressed = compressor.compress(state.paperText);  // compress the whole paper

    KeyManager keyMgr;
    string rootKey = keyMgr.generateRootKey();
    string encrypted = keyMgr.encrypt(compressed, rootKey);    // encrypt the compressed bytes with AES-256-GCM

    PaperPackage pkg;
    pkg.bitCount = compressor.getBitCount();
    pkg.unlockTimestamp = (int64_t)unlockTs;                   // embed scheduled unlock time in v3 package
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

    // Cache locked package and root key for Phase 2 & Phase 3 routes, resetting stale distribution state.
    state.lockedPkg = pkg;
    state.rootKey = rootKey;
    state.scheduledUnlockTime = unlockTs;                      // update server-side scheduled unlock time
    state.broadcastDone = false;
    state.hops.clear();
    state.auditLog.reset();
    state.tamperedIndex = -1;
    state.originalToNode = "";

    reply(res, 200, json{
        {"originalBits", (int)state.paperText.size() * 8},
        {"compressedBits", pkg.bitCount},
        {"compressedBytes", (int)compressed.size()},
        {"encryptedBytes", (int)encrypted.size()},
        {"packageFile", packagePath},
        {"verified", verified},
        {"unlockTimestamp", (int64_t)unlockTs},
        {"unlockTimeFormatted", TimeLock::formatTime(unlockTs)}
    });
}

// ---------------------------------------------------------------------------
// Phase 2 & Phase 3 helpers (called with stateMutex locked)
// ---------------------------------------------------------------------------

// Converts raw binary key bytes into an 8-character public hex fingerprint prefix.
string shortFingerprint(const string& rawKey) {
    if (rawKey.empty()) return "";
    ostringstream out;
    out << hex << setfill('0');
    size_t limit = min((size_t)4, rawKey.size());
    for (size_t i = 0; i < limit; i++) {
        out << setw(2) << (int)(unsigned char)rawKey[i];
    }
    return out.str();
}

// Adds a single node into both the AVLRegistry and LCRSTree.
void addDistNode(AVLRegistry& reg, LCRSTree& tree, const string& parentId,
                 const string& nodeId, const string& name,
                 const string& role, const string& status) {
    reg.registerNode(nodeId, name, role, parentId, status, "");
    if (parentId.empty()) {
        tree.setRoot(nodeId, name, role);
    } else {
        tree.addChild(parentId, nodeId, name, role);
    }
}

// Initializes the All-India hierarchy (Board + 36 States/UTs + District/City/Centre chains) if not created yet.
void ensureNetworkInitialized() {
    if (state.registry && state.tree) return;
    state.registry = make_unique<AVLRegistry>();
    state.tree = make_unique<LCRSTree>();
    state.auditLog = make_unique<AuditLog>();

    populateAllIndiaNetwork(*state.registry, *state.tree);     // populate Board, 28 States, 8 UTs, and Centres
}

// Loads a 32-byte key file into outKey.
bool loadServerKeyFile(const string& path, string& outKey) {
    ifstream in(path, ios::binary);
    if (!in.is_open()) return false;
    outKey.assign((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    return (int)outKey.size() == AES_KEY_BYTES;
}

// Ensures state.lockedPkg and state.rootKey are available (from memory, output/, or project root).
bool ensurePackageAvailable(string& err) {
    if (!state.lockedPkg.encrypted.empty() && (int)state.rootKey.size() == AES_KEY_BYTES) {
        return true;
    }
    const vector<pair<string, string>> candidates = {
        {OUTPUT_DIR + "/paper.scbt", OUTPUT_DIR + "/paper.key"},
        {"paper.scbt", "paper.key"}
    };
    for (const auto& pairPaths : candidates) {
        PaperPackage pkg;
        string key;
        string loadErr;
        if (loadPaperPackage(pairPaths.first, pkg, loadErr) && loadServerKeyFile(pairPaths.second, key)) {
            state.lockedPkg = pkg;
            state.rootKey = key;
            return true;
        }
    }
    err = "No locked paper package found. Complete Phase 1 (Upload -> Configure -> Preview -> Lock) first.";
    return false;
}

// Builds JSON summary of the SHA-256 hash-chained AuditLog.
json buildAuditJson() {
    if (!state.auditLog) {
        return json{{"intact", true}, {"brokenAt", -1}, {"entries", json::array()}};
    }
    int brokenAt = state.auditLog->verify();
    json entries = json::array();
    for (const LogEntry& e : state.auditLog->getEntries()) {
        entries.push_back({
            {"index", e.index},
            {"fromNode", e.fromNode},
            {"toNode", e.toNode},
            {"timestamp", e.timestamp},
            {"prevHash", e.prevHash.substr(0, 12)},
            {"hash", e.hash.substr(0, 12)}
        });
    }
    return json{
        {"intact", brokenAt == -1},
        {"brokenAt", brokenAt},
        {"tamperedIndex", state.tamperedIndex},
        {"entries", entries}
    };
}

// Clamps allocated exam duration between 30 seconds (for quick live demo) and 5 hours (18000s).
int clampExamDurationSeconds(int seconds) {
    return max(30, min(18000, seconds));                       // enforce safe lower/upper bounds
}

// Returns the exam duration allocated for a centre, or 2700s (45 mins) by default.
int resolveCentreDurationSeconds(const string& centreId) {
    auto it = state.centreDurationSeconds.find(centreId);      // check if this centre set a custom timer
    return (it != state.centreDurationSeconds.end()) ? it->second : 2700;
}

// Converts one StudentAttemptStatus struct into a JSON object for API responses.
json attemptToJson(const StudentAttemptStatus& a) {
    return json{
        {"centreId", a.centreId},                              // exam centre holding this student
        {"studentRoll", a.studentRoll},                        // student's roll number
        {"status", a.status},                                  // Forwarded / Attempting / Submitted
        {"answeredCount", a.answeredCount},                    // how many questions answered
        {"reviewCount", a.reviewCount},                        // how many marked for review
        {"totalQuestions", a.totalQuestions},                  // total questions in shuffled paper
        {"exitWarnings", a.exitWarnings},                      // fullscreen exit proctor count
        {"timeRemainingSeconds", a.timeRemainingSeconds},      // seconds remaining on exam clock
        {"autoSubmitted", a.autoSubmitted},                    // true if clock expiry triggered submit
        {"updatedAt", a.updatedAt}                             // formatted server timestamp
    };
}

// Builds the complete Phase 2 network, AVL stats, hops, and audit JSON response.
json buildNetworkStateJson() {
    ensureNetworkInitialized();
    string pkgErr;
    bool hasPackage = ensurePackageAvailable(pkgErr);
    if (state.scheduledUnlockTime == 0) {                      // initialize unlock time from package or default +30 min
        state.scheduledUnlockTime = (hasPackage && state.lockedPkg.unlockTimestamp > 0)
            ? (time_t)state.lockedPkg.unlockTimestamp
            : (time(nullptr) + 1800);
    }

    json nodesJson = json::array();
    for (const NodeInfo& info : state.registry->getAllNodes()) {
        const LCRSNode* lNode = state.tree->getNode(info.nodeId);
        bool delivered = (lNode != nullptr) && lNode->delivered;
        int payloadBytes = (lNode != nullptr && delivered) ? (int)lNode->encryptedPayload.size() : 0;
        nodesJson.push_back({
            {"nodeId", info.nodeId},
            {"name", info.name},
            {"role", info.role},
            {"parentId", info.parentId},
            {"status", info.status},
            {"keyFingerprint", info.keyFingerprint},
            {"delivered", delivered},
            {"payloadBytes", payloadBytes}
        });
    }

    json hopsJson = json::array();
    for (const HopReport& h : state.hops) {
        hopsJson.push_back({
            {"hopNumber", h.hopNumber},
            {"fromId", h.fromId},
            {"toId", h.toId},
            {"status", h.status},
            {"payloadBytes", h.payloadBytes}
        });
    }

    json durationsJson = json::object();                       // map each centreId -> allocated duration seconds
    for (const auto& kv : state.centreDurationSeconds) {
        durationsJson[kv.first] = kv.second;                   // store allocated exam timer per centre
    }

    json attemptsJson = json::array();                         // list of live candidate terminal statuses
    for (const auto& kv : state.studentAttempts) {
        attemptsJson.push_back(attemptToJson(kv.second));    // append serialized student attempt status
    }

    return json{
        {"totalNodes", state.registry->size()},
        {"avlHeight", state.registry->height()},
        {"isBalanced", state.registry->isBalanced()},
        {"avlRootId", state.registry->rootId()},
        {"hasPackage", hasPackage},
        {"packageEncryptedBytes", hasPackage ? (int)state.lockedPkg.encrypted.size() : 0},
        {"packageCompressedBits", hasPackage ? state.lockedPkg.bitCount : 0},
        {"broadcastDone", state.broadcastDone},
        {"unlockTimestamp", (int64_t)state.scheduledUnlockTime},
        {"unlockTimeFormatted", TimeLock::formatTime(state.scheduledUnlockTime)},
        {"nodes", nodesJson},
        {"hops", hopsJson},
        {"audit", buildAuditJson()},
        {"centreDurations", durationsJson},
        {"studentAttempts", attemptsJson}
    };
}

// Builds the TimeLock Min-Heap schedule matching the current exam centres.
TimeLock buildCentreSchedule(time_t baseNow, vector<ExamSlot>& orderedOut) {
    TimeLock tl;
    tl.addSlot("SLOT-1", "CTR-HR-01", "Morning Exam Shift (09:00)",   baseNow + 1800);
    tl.addSlot("SLOT-2", "CTR-AP-01", "Afternoon CSE Shift (14:00)", baseNow + 3600);
    tl.addSlot("SLOT-3", "CTR-AS-01", "Evening Lab Shift (17:00)",   baseNow + 7200);
    // Include any dynamically registered Registered centres in the schedule as well
    if (state.registry) {
        int extraIdx = 4;
        for (const NodeInfo& n : state.registry->getAllNodes()) {
            if (n.role == "Centre" && n.status == "Registered"
                && n.nodeId != "CTR-HR-01" && n.nodeId != "CTR-AP-01" && n.nodeId != "CTR-AS-01") {
                tl.addSlot("SLOT-" + to_string(extraIdx++), n.nodeId, n.name + " Shift", baseNow + 1800);
            }
        }
    }
    orderedOut = tl.getAllSlotsSorted();
    return tl;
}

// ---------------------------------------------------------------------------
// GET /api/phase2/network
// ---------------------------------------------------------------------------
void handlePhase2Network(const httplib::Request&, httplib::Response& res) {
    lock_guard<mutex> lock(stateMutex);
    reply(res, 200, buildNetworkStateJson());
}

// ---------------------------------------------------------------------------
// POST /api/phase2/register
// Registers a new centre OR updates an existing node's status in AVLRegistry.
// Body: {"parentId":"ST-08-D01-CT","nodeId":"CTR-HR-02","name":"...","role":"Centre","status":"Registered"}
// ---------------------------------------------------------------------------
void handlePhase2Register(const httplib::Request& req, httplib::Response& res) {
    json input;
    try { input = json::parse(req.body); }
    catch (...) { replyError(res, 400, "Invalid JSON"); return; }

    if (!input.contains("nodeId") || !input["nodeId"].is_string()
        || !input.contains("status") || !input["status"].is_string()) {
        replyError(res, 400, "nodeId and status are required");
        return;
    }

    string nodeId = input["nodeId"].get<string>();
    string status = input["status"].get<string>();
    if (status != "Registered" && status != "Pending" && status != "Rejected") {
        replyError(res, 400, "status must be Registered, Pending, or Rejected");
        return;
    }

    lock_guard<mutex> lock(stateMutex);
    ensureNetworkInitialized();

    const NodeInfo* existing = state.registry->find(nodeId);
    if (existing != nullptr) {
        if (nodeId == "BOARD" && status != "Registered") {
            replyError(res, 400, "Root BOARD must remain Registered");
            return;
        }
        state.registry->registerNode(existing->nodeId, existing->name, existing->role,
                                     existing->parentId, status, existing->keyFingerprint);
    } else {
        string parentId = input.value("parentId", "");
        string name = input.value("name", nodeId);
        string role = input.value("role", "Centre");
        if (parentId.empty() || state.tree->getNode(parentId) == nullptr) {
            replyError(res, 400, "Valid parentId in the LCRS hierarchy is required");
            return;
        }
        addDistNode(*state.registry, *state.tree, parentId, nodeId, name, role, status);
    }

    state.broadcastDone = false;
    state.hops.clear();
    state.tamperedIndex = -1;
    state.originalToNode = "";
    reply(res, 200, buildNetworkStateJson());
}

// ---------------------------------------------------------------------------
// POST /api/phase2/broadcast
// Runs level-order BFS broadcast across LCRSTree with AVLRegistry gatekeeping,
// per-hop AES-256-GCM onion wrapping, and SHA-256 AuditLog chaining.
// ---------------------------------------------------------------------------
void handlePhase2Broadcast(const httplib::Request&, httplib::Response& res) {
    lock_guard<mutex> lock(stateMutex);
    ensureNetworkInitialized();

    string pkgErr;
    if (!ensurePackageAvailable(pkgErr)) {
        replyError(res, 409, pkgErr);
        return;
    }

    KeyManager keyMgr;
    state.auditLog = make_unique<AuditLog>();
    state.tamperedIndex = -1;
    state.originalToNode = "";
    state.studentAttempts.clear();                             // reset old candidate attempts on a new broadcast
    state.hops = state.tree->bfsBroadcast(state.lockedPkg.encrypted, state.rootKey,
                                          *state.registry, keyMgr, *state.auditLog);
    state.broadcastDone = true;

    // Record safe 8-char key fingerprints in AVLRegistry for all delivered nodes
    for (const NodeInfo& info : state.registry->getAllNodes()) {
        const LCRSNode* lNode = state.tree->getNode(info.nodeId);
        string fp = (lNode != nullptr && lNode->delivered) ? shortFingerprint(lNode->key) : "";
        state.registry->registerNode(info.nodeId, info.name, info.role, info.parentId, info.status, fp);
    }

    reply(res, 200, buildNetworkStateJson());
}

// ---------------------------------------------------------------------------
// POST /api/phase2/audit
// Body: {"action":"tamper","index":3} | {"action":"restore"} | {"action":"verify"}
// ---------------------------------------------------------------------------
void handlePhase2Audit(const httplib::Request& req, httplib::Response& res) {
    json input;
    try { input = json::parse(req.body); }
    catch (...) { replyError(res, 400, "Invalid JSON"); return; }

    string action = input.value("action", "verify");

    lock_guard<mutex> lock(stateMutex);
    ensureNetworkInitialized();
    if (!state.broadcastDone || !state.auditLog || state.auditLog->getEntries().empty()) {
        replyError(res, 409, "Run BFS broadcast first to create transfer log entries");
        return;
    }

    if (action == "tamper") {
        int idx = input.value("index", 3);
        auto& entries = state.auditLog->getMutableEntries();
        if (idx < 0 || idx >= (int)entries.size()) {
            idx = (int)entries.size() - 1;
        }
        if (state.tamperedIndex == -1) {
            state.tamperedIndex = idx;
            state.originalToNode = entries[idx].toNode;
        }
        entries[state.tamperedIndex].toNode = "ROGUE-NODE";
    } else if (action == "restore") {
        if (state.tamperedIndex != -1 && state.tamperedIndex < (int)state.auditLog->getEntries().size()) {
            state.auditLog->getMutableEntries()[state.tamperedIndex].toNode = state.originalToNode;
        }
        state.tamperedIndex = -1;
        state.originalToNode = "";
    }

    reply(res, 200, buildAuditJson());
}

// ---------------------------------------------------------------------------
// POST /api/phase3/unlock
// Enforces TimeLock Min-Heap, peels all 5 AES-256-GCM onion layers,
// Huffman-decompresses, and shuffles questions deterministically per studentId.
// Body: {"centreId":"CTR-HR-01","studentId":"2510012263","simulateEarly":false}
// ---------------------------------------------------------------------------
void handlePhase3Unlock(const httplib::Request& req, httplib::Response& res) {
    json input;
    try { input = json::parse(req.body); }
    catch (...) { replyError(res, 400, "Invalid JSON"); return; }

    string centreId = input.value("centreId", "CTR-HR-01");
    string studentId = input.value("studentId", "2510012263");
    bool simulateEarly = input.value("simulateEarly", false);

    lock_guard<mutex> lock(stateMutex);
    ensureNetworkInitialized();
    if (!state.broadcastDone) {
        replyError(res, 409, "Run Phase 2 BFS broadcast before unlocking at an exam centre");
        return;
    }

    time_t baseNow = time(nullptr);
    vector<ExamSlot> orderedSlots;
    TimeLock timeLock = buildCentreSchedule(baseNow, orderedSlots);

    time_t targetUnlock = baseNow + 1800;
    for (const ExamSlot& s : orderedSlots) {
        if (s.centreId == centreId) {
            targetUnlock = s.unlockTime;
            break;
        }
    }
    time_t simulatedClock = simulateEarly ? (targetUnlock - 1800) : targetUnlock;

    json slotsJson = json::array();
    for (const ExamSlot& s : orderedSlots) {
        slotsJson.push_back({
            {"slotId", s.slotId},
            {"centreId", s.centreId},
            {"examName", s.examName},
            {"unlockTimeFormatted", TimeLock::formatTime(s.unlockTime)},
            {"unlockedAtClock", timeLock.canUnlock(s.unlockTime, simulatedClock)},
            {"secondsRemaining", timeLock.secondsUntilUnlock(s.unlockTime, simulatedClock)}
        });
    }

    const LCRSNode* cNode = state.tree->getNode(centreId);
    int encryptedBytes = (cNode != nullptr && cNode->delivered) ? (int)cNode->encryptedPayload.size() : 0;

    KeyManager keyMgr;
    HuffmanCompressor compressor;
    ExamCentre examCentre;
    string unlockedPlaintext;
    string unlockError;

    bool ok = examCentre.unlockAndDecompress(
        centreId, targetUnlock, simulatedClock, timeLock, *state.tree,
        keyMgr, compressor, state.lockedPkg, unlockedPlaintext, unlockError);

    if (!ok) {
        reply(res, 200, json{
            {"unlocked", false},
            {"centreId", centreId},
            {"studentId", studentId},
            {"simulateEarly", simulateEarly},
            {"currentClockFormatted", TimeLock::formatTime(simulatedClock)},
            {"targetUnlockFormatted", TimeLock::formatTime(targetUnlock)},
            {"blockedReason", unlockError},
            {"encryptedBytes", encryptedBytes},
            {"slots", slotsJson}
        });
        return;
    }

    StudentView sView = examCentre.buildStudentTerminalPaper(centreId, studentId, unlockedPlaintext);

    // Count onion layers peeled along the parent path
    int layersPeeled = 0;
    const LCRSNode* cursor = cNode;
    while (cursor != nullptr) {
        layersPeeled++;
        cursor = cursor->parent;
    }

    reply(res, 200, json{
        {"unlocked", true},
        {"centreId", centreId},
        {"studentId", sView.studentId},
        {"shuffleSeed", sView.shuffleSeed},
        {"simulateEarly", simulateEarly},
        {"currentClockFormatted", TimeLock::formatTime(simulatedClock)},
        {"targetUnlockFormatted", TimeLock::formatTime(targetUnlock)},
        {"layersPeeled", layersPeeled},
        {"encryptedBytes", encryptedBytes},
        {"compressedBytes", (state.lockedPkg.bitCount + 7) / 8},
        {"plainBytes", (int)unlockedPlaintext.size()},
        {"questionCount", (int)sView.shuffledQuestions.size()},
        {"shuffledQuestions", sView.shuffledQuestions},
        {"formattedPaper", sView.formattedPaper},
        {"slots", slotsJson}
    });
}

// ---------------------------------------------------------------------------
// POST /api/phase3/trace
// Simulates a leaked encrypted paper from 'simulatedLeakNodeId' and traces it
// back to its source node via AVLRegistry O(log n) lookup and LCRS custody path.
// Body: {"simulatedLeakNodeId":"CTR-AP-01"}
// ---------------------------------------------------------------------------
void handlePhase3Trace(const httplib::Request& req, httplib::Response& res) {
    json input;
    try { input = json::parse(req.body); }
    catch (...) { replyError(res, 400, "Invalid JSON"); return; }

    string leakNodeId = input.value("simulatedLeakNodeId", "CTR-AP-01");

    lock_guard<mutex> lock(stateMutex);
    ensureNetworkInitialized();
    if (!state.broadcastDone) {
        replyError(res, 409, "Run Phase 2 BFS broadcast before tracing leaked ciphertexts");
        return;
    }

    const LCRSNode* leakedNode = state.tree->getNode(leakNodeId);
    if (leakedNode == nullptr || !leakedNode->delivered) {
        replyError(res, 400, "Selected node (" + leakNodeId + ") did not receive an encrypted paper (it may be Pending/Rejected)");
        return;
    }

    LeakTracer tracer;
    LeakReport report = tracer.traceLeakedCiphertext(leakedNode->encryptedPayload, *state.tree, *state.registry);

    reply(res, 200, json{
        {"traced", report.traced},
        {"leakedSha256", report.leakedSha256},
        {"leakedBytes", report.leakedBytes},
        {"culpritNodeId", report.culpritNodeId},
        {"culpritName", report.culpritName},
        {"culpritRole", report.culpritRole},
        {"statusInRegistry", report.statusInRegistry},
        {"keyFingerprint", report.keyFingerprint},
        {"hierarchyDepth", report.hierarchyDepth},
        {"chainOfCustody", report.chainOfCustody}
    });
}

// ---------------------------------------------------------------------------
// POST /api/phase2/schedule
// Body: {"unlockTimestamp": <sec>} or {"minutesFromNow": <int>}
// Updates state.scheduledUnlockTime and re-saves output/paper.scbt if present.
// ---------------------------------------------------------------------------
void handlePhase2Schedule(const httplib::Request& req, httplib::Response& res) {
    json input = json::parse(req.body, nullptr, false);
    if (input.is_discarded()) {
        replyError(res, 400, "Send a JSON body");
        return;
    }

    lock_guard<mutex> lock(stateMutex);
    ensureNetworkInitialized();

    time_t newUnlock = 0;
    if (input.contains("unlockTimestamp") && input["unlockTimestamp"].is_number_integer()) {
        newUnlock = (time_t)input["unlockTimestamp"].get<int64_t>();
    } else if (input.contains("minutesFromNow") && input["minutesFromNow"].is_number_integer()) {
        newUnlock = time(nullptr) + (time_t)(input["minutesFromNow"].get<int64_t>() * 60);
    } else {
        replyError(res, 400, "Provide unlockTimestamp or minutesFromNow");
        return;
    }

    state.scheduledUnlockTime = newUnlock;                     // update server-side scheduled unlock time
    state.lockedPkg.unlockTimestamp = (int64_t)newUnlock;      // update in-memory package timestamp

    string pkgPath = OUTPUT_DIR + "/paper.scbt";
    if (!state.lockedPkg.encrypted.empty() && fs::exists(pkgPath)) {
        string saveErr;
        savePaperPackage(pkgPath, state.lockedPkg, saveErr);   // persist updated timestamp to v3 .scbt file
    }

    reply(res, 200, buildNetworkStateJson());
}

// Extracts either examDurationSeconds or examDurationMinutes from a JSON payload and clamps it.
bool parseDurationInput(const json& input, int& outSeconds) {
    if (input.contains("examDurationSeconds") && input["examDurationSeconds"].is_number_integer()) {
        outSeconds = clampExamDurationSeconds(input["examDurationSeconds"].get<int>()); // clamp seconds
        return true;
    }
    if (input.contains("examDurationMinutes") && input["examDurationMinutes"].is_number_integer()) {
        int mins = max(1, min(300, input["examDurationMinutes"].get<int>())); // clamp 1..300 minutes
        outSeconds = clampExamDurationSeconds(mins * 60);                     // convert to clamped seconds
        return true;
    }
    return false;                                              // neither field was supplied
}

// ---------------------------------------------------------------------------
// POST /api/centre/forward
// Body: {"centreId": "CTR-UK-01", "studentRolls": ["2510013438", ...], "examDurationSeconds": 2700}
// Forwards the L4-wrapped paper from centreId to each student terminal (L5).
// ---------------------------------------------------------------------------
void handleCentreForward(const httplib::Request& req, httplib::Response& res) {
    json input = json::parse(req.body, nullptr, false);
    if (input.is_discarded() || !input.contains("centreId") || !input.contains("studentRolls")
        || !input["studentRolls"].is_array()) {
        replyError(res, 400, "Send centreId and studentRolls array");
        return;
    }

    string centreId = input["centreId"].get<string>();
    vector<string> studentRolls;
    for (const auto& r : input["studentRolls"]) {
        if (r.is_string() && !r.get<string>().empty()) {
            studentRolls.push_back(r.get<string>());
        }
    }

    lock_guard<mutex> lock(stateMutex);
    ensureNetworkInitialized();
    if (!state.broadcastDone) {
        replyError(res, 409, "Run Phase 2 broadcast first");
        return;
    }

    const LCRSNode* centreNode = state.tree->getNode(centreId);
    if (centreNode == nullptr || !centreNode->delivered) {
        replyError(res, 409, "Centre " + centreId + " has not received the encrypted paper");
        return;
    }

    int requestedDur = 0;                                      // optional exam timer supplied by centre
    if (parseDurationInput(input, requestedDur)) {
        state.centreDurationSeconds[centreId] = requestedDur;  // save centre's chosen exam duration
    } else if (state.centreDurationSeconds.find(centreId) == state.centreDurationSeconds.end()) {
        state.centreDurationSeconds[centreId] = 2700;          // initialize default 45 minutes if unset
    }
    int durationSec = resolveCentreDurationSeconds(centreId);  // active duration for forwarded students

    KeyManager keyMgr;
    ExamCentre examCentre;
    vector<StudentTerminalReceipt> receipts = examCentre.forwardPaperToStudents(
        centreId, studentRolls, *state.registry, *state.tree, keyMgr, *state.auditLog);

    string nowFormatted = TimeLock::formatTime(time(nullptr)); // timestamp when paper reached student queue
    json receiptsJson = json::array();
    for (const auto& rc : receipts) {
        if (rc.forwarded) {                                    // record initial live attempt status as Forwarded
            string key = centreId + ":" + rc.studentRoll;
            auto existing = state.studentAttempts.find(key);
            if (existing == state.studentAttempts.end() || existing->second.status == "Forwarded") {
                StudentAttemptStatus st;
                st.centreId = centreId;
                st.studentRoll = rc.studentRoll;
                st.status = "Forwarded";
                st.timeRemainingSeconds = durationSec;
                st.updatedAt = nowFormatted;
                state.studentAttempts[key] = st;
            }
        }
        receiptsJson.push_back({
            {"studentNodeId", rc.studentNodeId},
            {"studentRoll", rc.studentRoll},
            {"centreId", rc.centreId},
            {"forwarded", rc.forwarded},
            {"payloadBytes", rc.payloadBytes},
            {"keyFingerprint", rc.keyFingerprint}
        });
    }

    reply(res, 200, json{
        {"centreId", centreId},
        {"examDurationSeconds", durationSec},
        {"receipts", receiptsJson},
        {"network", buildNetworkStateJson()}
    });
}

// ---------------------------------------------------------------------------
// POST /api/centre/duration
// Body: {"centreId": "CTR-UK-01", "examDurationSeconds": 1800}
// Allocates or updates exam countdown duration for a specific Exam Centre.
// ---------------------------------------------------------------------------
void handleCentreDuration(const httplib::Request& req, httplib::Response& res) {
    json input = json::parse(req.body, nullptr, false);        // parse JSON body safely
    if (input.is_discarded() || !input.contains("centreId") || !input["centreId"].is_string()) {
        replyError(res, 400, "centreId is required");
        return;
    }

    string centreId = input["centreId"].get<string>();         // target exam centre node ID
    int durationSec = 0;                                       // validated duration in seconds
    if (centreId.empty() || !parseDurationInput(input, durationSec)) {
        replyError(res, 400, "Provide valid centreId and examDurationSeconds or examDurationMinutes");
        return;
    }

    lock_guard<mutex> lock(stateMutex);                        // protect shared state under mutex
    ensureNetworkInitialized();
    state.centreDurationSeconds[centreId] = durationSec;       // store updated exam timer for this centre

    // Propagate new duration to waiting students who have not started attempting yet.
    string nowFormatted = TimeLock::formatTime(time(nullptr));
    for (auto& kv : state.studentAttempts) {
        if (kv.second.centreId == centreId && kv.second.status == "Forwarded") {
            kv.second.timeRemainingSeconds = durationSec;
            kv.second.updatedAt = nowFormatted;
        }
    }

    reply(res, 200, buildNetworkStateJson());                  // return refreshed network + durations
}

// ---------------------------------------------------------------------------
// POST /api/student/portal
// Body: {"centreId": "CTR-UK-01", "studentRoll": "2510013438", "simulateUnlocked": false}
// Checks if student terminal received L5 paper and whether TimeLock has expired.
// ---------------------------------------------------------------------------
void handleStudentPortal(const httplib::Request& req, httplib::Response& res) {
    json input = json::parse(req.body, nullptr, false);
    if (input.is_discarded()) {
        replyError(res, 400, "Send a JSON body");
        return;
    }

    string centreId = input.value("centreId", "CTR-UK-01");
    string studentRoll = input.value("studentRoll", "");
    bool simulateUnlocked = input.value("simulateUnlocked", false);
    if (studentRoll.empty()) {
        replyError(res, 400, "studentRoll is required");
        return;
    }

    lock_guard<mutex> lock(stateMutex);
    ensureNetworkInitialized();
    string pkgErr;
    bool hasPackage = ensurePackageAvailable(pkgErr);
    if (state.scheduledUnlockTime == 0) {
        state.scheduledUnlockTime = (hasPackage && state.lockedPkg.unlockTimestamp > 0)
            ? (time_t)state.lockedPkg.unlockTimestamp
            : (time(nullptr) + 1800);
    }

    time_t unlockTs = state.scheduledUnlockTime;
    time_t currentClock = simulateUnlocked ? unlockTs : time(nullptr);
    string unlockFmt = TimeLock::formatTime(unlockTs);
    string clockFmt = TimeLock::formatTime(currentClock);
    int durationSec = resolveCentreDurationSeconds(centreId);  // allocated duration set by this exam centre

    string stuNodeId = "STU-" + studentRoll;
    const LCRSNode* stuNode = state.tree->getNode(stuNodeId);
    if (stuNode == nullptr || !stuNode->delivered) {
        reply(res, 200, json{
            {"forwarded", false},
            {"locked", true},
            {"centreId", centreId},
            {"studentRoll", studentRoll},
            {"examDurationSeconds", durationSec},
            {"unlockTimestamp", (int64_t)unlockTs},
            {"unlockTimeFormatted", unlockFmt},
            {"currentClockFormatted", clockFmt},
            {"message", "Waiting for Exam Centre to forward encrypted paper."}
        });
        return;
    }

    KeyManager keyMgr;
    string fp = shortFingerprint(stuNode->key);
    int encryptedBytes = (int)stuNode->encryptedPayload.size();

    if (currentClock < unlockTs) {
        long secRem = (long)(unlockTs - currentClock);
        reply(res, 200, json{
            {"forwarded", true},
            {"locked", true},
            {"centreId", centreId},
            {"studentRoll", studentRoll},
            {"studentNodeId", stuNodeId},
            {"examDurationSeconds", durationSec},
            {"encryptedBytes", encryptedBytes},
            {"onionLayers", 6},
            {"keyFingerprint", fp},
            {"unlockTimestamp", (int64_t)unlockTs},
            {"unlockTimeFormatted", unlockFmt},
            {"currentClockFormatted", clockFmt},
            {"secondsRemaining", secRem}
        });
        return;
    }

    if (!hasPackage) {
        replyError(res, 409, pkgErr);
        return;
    }

    string peeledCompressed;
    if (!state.tree->unwrapAtNode(stuNodeId, keyMgr, peeledCompressed)) {
        replyError(res, 409, "Onion decryption failed at " + stuNodeId);
        return;
    }

    HuffmanCompressor compressor;
    string plaintext = compressor.decompress(peeledCompressed, state.lockedPkg.bitCount, state.lockedPkg.codes);
    if (plaintext.empty()) {
        replyError(res, 500, "Huffman decompression produced empty output");
        return;
    }

    ExamCentre examCentre;
    StudentView view = examCentre.buildStudentTerminalPaper(centreId, stuNodeId, plaintext);

    reply(res, 200, json{
        {"forwarded", true},
        {"locked", false},
        {"centreId", centreId},
        {"studentRoll", studentRoll},
        {"studentNodeId", stuNodeId},
        {"examDurationSeconds", durationSec},
        {"encryptedBytes", encryptedBytes},
        {"layersPeeled", 6},
        {"compressedBytes", (int)peeledCompressed.size()},
        {"plainBytes", (int)plaintext.size()},
        {"keyFingerprint", fp},
        {"shuffleSeed", view.shuffleSeed},
        {"questionCount", (int)view.shuffledQuestions.size()},
        {"shuffledQuestions", view.shuffledQuestions},
        {"formattedPaper", view.formattedPaper},
        {"unlockTimestamp", (int64_t)unlockTs},
        {"unlockTimeFormatted", unlockFmt},
        {"currentClockFormatted", clockFmt}
    });
}

// ---------------------------------------------------------------------------
// POST /api/student/status
// Body: {"centreId":"CTR-UK-01","studentRoll":"2510012263","status":"Attempting",...}
// Receives live attempt progress, fullscreen proctor warnings, and final submit state.
// ---------------------------------------------------------------------------
void handleStudentStatus(const httplib::Request& req, httplib::Response& res) {
    json input = json::parse(req.body, nullptr, false);        // parse JSON payload
    if (input.is_discarded()) {
        replyError(res, 400, "Send a JSON body");
        return;
    }

    string centreId = input.value("centreId", "");
    string studentRoll = input.value("studentRoll", "");
    string status = input.value("status", "Attempting");
    if (centreId.empty() || studentRoll.empty()) {             // validate student identifiers
        replyError(res, 400, "centreId and studentRoll are required");
        return;
    }
    if (status != "Forwarded" && status != "Attempting" && status != "Submitted") {
        replyError(res, 400, "status must be Forwarded, Attempting, or Submitted");
        return;
    }

    lock_guard<mutex> lock(stateMutex);                        // serialize shared state update
    ensureNetworkInitialized();

    StudentAttemptStatus st;                                   // build updated status record
    st.centreId = centreId;
    st.studentRoll = studentRoll;
    st.status = status;
    st.answeredCount = max(0, input.value("answeredCount", 0));
    st.reviewCount = max(0, input.value("reviewCount", 0));
    st.totalQuestions = max(0, input.value("totalQuestions", 0));
    st.exitWarnings = max(0, input.value("exitWarnings", 0));
    st.timeRemainingSeconds = max(0, input.value("timeRemainingSeconds", resolveCentreDurationSeconds(centreId)));
    st.autoSubmitted = input.value("autoSubmitted", false);
    st.updatedAt = TimeLock::formatTime(time(nullptr));

    string key = centreId + ":" + studentRoll;                 // unique candidate tracking key
    state.studentAttempts[key] = st;

    reply(res, 200, json{
        {"ok", true},
        {"attempt", attemptToJson(st)}
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

    // Phase 2 & Phase 3 routes
    svr.Get("/api/phase2/network", handlePhase2Network);
    svr.Post("/api/phase2/register", handlePhase2Register);
    svr.Post("/api/phase2/broadcast", handlePhase2Broadcast);
    svr.Post("/api/phase2/audit", handlePhase2Audit);
    svr.Post("/api/phase2/schedule", handlePhase2Schedule);
    svr.Post("/api/centre/duration", handleCentreDuration);
    svr.Post("/api/centre/forward", handleCentreForward);
    svr.Post("/api/student/portal", handleStudentPortal);
    svr.Post("/api/student/status", handleStudentStatus);
    svr.Post("/api/phase3/unlock", handlePhase3Unlock);
    svr.Post("/api/phase3/trace", handlePhase3Trace);

    // Any exception inside a handler becomes a 500 reply instead of killing the server.
    svr.set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr ep) {
        string msg = "Internal server error";
        try { if (ep) rethrow_exception(ep); }
        catch (const exception& e) { msg += string(": ") + e.what(); }
        catch (...) {}
        replyError(res, 500, msg);
    });

    const char* host = "0.0.0.0";                              // listen on all network interfaces (needed for Docker/Render/HF & phone Wi-Fi)
    const char* envPort = getenv("PORT");                      // cloud hosts (Render/Hugging Face) pass the port in the PORT variable
    int port = (envPort != nullptr) ? atoi(envPort) : 8080;    // use PORT if set, otherwise default to 8080 locally
    cout << "Secure CBT server running at http://localhost:" << port << endl;
    if (!svr.listen(host, port)) {                             // blocks here until the server stops
        cout << "Could not start the server (is port " << port << " already in use?)" << endl;
        return 1;
    }
    return 0;
}
