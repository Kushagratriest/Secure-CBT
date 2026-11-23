// TimeLock.h: Min-Heap Exam Slot Scheduler & Time-Locked Release for Phase 3.
// Stores upcoming exam slots in a Min-Heap ordered by scheduled unlockTime (earliest first)
// and enforces time-based access control so no exam centre can decrypt a paper early.

#pragma once                                                    // include this header only once per compilation
#include <iostream>                                             // cout, endl for console output
#include <string>                                               // std::string for slot/centre IDs and labels
#include <vector>                                               // std::vector to back the priority_queue and return sorted lists
#include <queue>                                                // std::priority_queue for Min-Heap slot management
#include <ctime>                                                // time_t, time(), localtime(), strftime() for wall-clock checks
#include <stdexcept>                                            // runtime_error if popping from an empty heap
using namespace std;                                            // use standard library names directly

// Represents one scheduled exam unlock slot for an exam centre.
struct ExamSlot {
    string slotId;                                              // unique slot ID, e.g. "SLOT-1"
    string centreId;                                            // exam centre ID, e.g. "CTR-HR-01"
    string examName;                                            // readable session label, e.g. "Morning Session (09:00)"
    time_t unlockTime;                                          // Unix timestamp (seconds since 1970) when decryption unlocks
};

// Comparator functor that flips std::priority_queue into a Min-Heap (earliest unlockTime on top).
struct ExamSlotCompare {
    bool operator()(const ExamSlot& a, const ExamSlot& b) const {
        return a.unlockTime > b.unlockTime;                     // smaller timestamp gets higher priority (comes out first)
    }
};

// Manages exam schedules using a Min-Heap and verifies time-lock gates before decryption.
class TimeLock {
private:
    priority_queue<ExamSlot, vector<ExamSlot>, ExamSlotCompare> minHeap; // Min-Heap storing all scheduled exam slots

public:
    // Creates an empty Min-Heap time-lock scheduler.
    TimeLock() {}

    // Inserts a new scheduled exam slot into the Min-Heap in O(log n) time.
    void addSlot(const string& slotId, const string& centreId, const string& examName, time_t unlockTime) {
        minHeap.push({slotId, centreId, examName, unlockTime}); // push slot; heap keeps earliest unlockTime at top()
    }

    // Returns true if there are no scheduled slots left in the heap.
    bool isEmpty() const {
        return minHeap.empty();                                 // true when size == 0
    }

    // Returns how many slots are currently queued in the Min-Heap.
    int size() const {
        return (int)minHeap.size();                             // current count of scheduled slots
    }

    // Peeks at the earliest upcoming exam slot without removing it in O(1) time.
    ExamSlot peekEarliest() const {
        if (minHeap.empty()) {                                  // guard against empty heap
            throw runtime_error("TimeLock: no exam slots scheduled");
        }
        return minHeap.top();                                   // return the slot with the smallest unlockTime
    }

    // Removes and returns the earliest upcoming exam slot in O(log n) time (Extract-Min).
    ExamSlot popEarliest() {
        if (minHeap.empty()) {                                  // guard against empty heap
            throw runtime_error("TimeLock: cannot pop from empty schedule");
        }
        ExamSlot earliest = minHeap.top();                      // copy the earliest slot from top of Min-Heap
        minHeap.pop();                                          // remove it and re-heapify remaining slots
        return earliest;                                        // return the extracted earliest slot
    }

    // Returns a copy of all scheduled slots sorted from earliest to latest (without emptying the heap).
    vector<ExamSlot> getAllSlotsSorted() const {
        auto copyHeap = minHeap;                                // make a local copy so the real Min-Heap stays intact
        vector<ExamSlot> sorted;                                // holds slots in ascending unlockTime order
        while (!copyHeap.empty()) {                             // extract one by one until empty
            sorted.push_back(copyHeap.top());                   // append earliest remaining slot
            copyHeap.pop();                                     // pop it from the copy
        }
        return sorted;                                          // return chronologically ordered schedule
    }

    // Checks whether the scheduled unlock time has been reached at 'currentTime'.
    bool canUnlock(time_t unlockTime, time_t currentTime) const {
        return currentTime >= unlockTime;                       // true only at or after scheduled exam start time
    }

    // Returns how many seconds remain until unlock (returns 0 if already unlocked).
    long secondsUntilUnlock(time_t unlockTime, time_t currentTime) const {
        if (currentTime >= unlockTime) return 0;                // already unlocked, 0 seconds remaining
        return (long)(unlockTime - currentTime);                // remaining seconds locked
    }

    // Formats a Unix timestamp as readable local date-time ("YYYY-MM-DD HH:MM:SS").
    static string formatTime(time_t t) {
        tm* local = localtime(&t);                              // break Unix timestamp into calendar fields
        if (local == nullptr) return "1970-01-01 00:00:00";     // fallback string if localtime fails
        char buf[32];                                           // buffer for formatted date-time string
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", local); // format as YYYY-MM-DD HH:MM:SS
        return string(buf);                                     // return C++ string
    }
};
