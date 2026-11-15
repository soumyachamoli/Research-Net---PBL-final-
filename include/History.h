#ifndef HISTORY_H
#define HISTORY_H

// One viewed paper in the browsing history.
struct HistoryNode
{
    int paperId;
    HistoryNode* next;
};

// Browsing history as a singly linked list. The newest paper is always at the head:
//   10 -> 7 -> 4 -> 2
// Opening a paper that is already in the list moves it to the front.
class History
{
private:
    HistoryNode* head;
    int count;
    static const int MAX_ITEMS = 30;    // the oldest entries are removed after 30

    void removeLast();

public:
    History();
    ~History();

    // The history owns its nodes, so it must not be copied.
    History(const History&) = delete;
    History& operator=(const History&) = delete;

    void addHistory(int paperId);
    void displayHistory() const;
    bool deleteHistoryItem(int paperId);
    void clearHistory();

    HistoryNode* getHead() const;
    int size() const;
};

#endif
