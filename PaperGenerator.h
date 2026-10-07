//loops through each topic, randomly picks perTopic questions from it,
// removes them so no repeats. That's the weighted random sampling without replacement.

#pragma once
#include <iostream>
#include <vector>
#include <cstdlib>
#include "TopicIndex.h"
#include "QuestionBank.h"
using namespace std;

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
};