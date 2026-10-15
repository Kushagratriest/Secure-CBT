//loops through each topic, randomly picks perTopic questions from it,
// removes them so no repeats. That's the weighted random sampling without replacement.

#pragma once
#include <iostream>
#include <vector>
#include <cstdlib>
#include "TopicIndex.h"
#include "QuestionBank.h"
using namespace std;

// One line of the user's choices on the configure screen:
// "from <subject> / <topic>, give me <count> questions worth <marks> marks each".
struct SelectionRequest {
    string subject;     // e.g. "CS"
    string topic;       // e.g. "DSA"
    int marks;          // e.g. 2  (which mark group inside the topic)
    int count;          // e.g. 3  (how many questions the user wants from that group)
};

class PaperGenerator {
private:

//It stores the IDs of questions that got selected for the final paper.
    vector<string> selectedIds;

public:
    PaperGenerator() {
        cout << "PaperGenerator created" << endl;
    }

    //function takes the topic index, a list of topics, and how many questions per topic you want.
    vector<string> generate(TopicIndex& topicIndex, vector<string> topics, int perTopic) {
        for (string topic : topics) {//loop through each topic one by one, like "DSA", "OOP", "Maths".
            vector<string> ids = topicIndex.getQuestionsByTopic(topic);//get all question IDs under that topic. So for "DSA" you get ["Q1", "Q3", "Q7"].
            int count = 0;
            while (count < perTopic && ids.size() > 0) {//keep picking until you've picked enough questions OR the topic runs out of questions.
                int r = rand() % ids.size();//pick a random index from the remaining IDs.
                selectedIds.push_back(ids[r]);
                ids.erase(ids.begin() + r);//remove it from the local list so it can't be picked again. This is the "without replacement" part.
                count++;
            }
        }
        return selectedIds;//return the full list of selected question IDs.
    }

    // -----------------------------------------------------------------
    // Selection-based generation (used by the "configure paper" screen).
    // The user says, for each (subject, topic, marks) group, how many
    // questions they want, e.g. CS / DSA / 2 marks -> 3 questions.
    // -----------------------------------------------------------------

    // Picks exactly 'count' random questions from each requested group,
    // with no repeats. Returns the chosen IDs in request order.
    // If any request is impossible (count < 0 or more than available),
    // nothing is picked: 'problems' gets one message per bad request and
    // an empty list is returned, so the caller never gets a half-built paper.
    vector<string> generateFromSelection(const TopicIndex& topicIndex,
                                         const vector<SelectionRequest>& requests,
                                         vector<string>& problems) {
        problems.clear();                                       // start with no problems

        // pass 1: check every request before picking anything
        for (const SelectionRequest& r : requests) {
            int available = topicIndex.countQuestions(r.subject, r.topic, r.marks);   // how many exist in this group
            if (r.count < 0 || r.count > available) {           // asked for a negative number, or more than exist
                problems.push_back(r.subject + " / " + r.topic + " / " + to_string(r.marks) + " marks: asked for "
                                   + to_string(r.count) + ", only " + to_string(available) + " available");
            }
        }
        if (!problems.empty()) return {};                       // at least one bad request: build nothing

        // pass 2: random picking without replacement
        vector<string> chosen;                                  // local list, so repeated calls don't pile up
        for (const SelectionRequest& r : requests) {
            vector<string> ids = topicIndex.getQuestions(r.subject, r.topic, r.marks);   // copy of this group's IDs
            for (int i = 0; i < r.count; i++) {                 // pick r.count of them
                int pick = rand() % ids.size();                 // random position in what's left
                chosen.push_back(ids[pick]);                    // take that question
                ids.erase(ids.begin() + pick);                  // remove it so it can't be picked again
            }
        }
        return chosen;
    }
};