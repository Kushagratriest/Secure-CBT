#pragma once
#include <iostream>
#include <string>
using namespace std;

class Question {
private:
    string questionId;
    string text;
    string topic;
    int difficulty;
    int marks;

public:
    Question(string id, string text, string topic, int difficulty, int marks) {
        this->questionId = id;
        this->text = text;
        this->topic = topic;
        this->difficulty = difficulty;
        this->marks = marks;
    }

    string getId()    { return questionId; }
    string getText()  { return text; }
    string getTopic() { return topic; }
    int getDifficulty() { return difficulty; }
    int getMarks()    { return marks; }
};