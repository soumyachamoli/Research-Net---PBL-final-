#include "LinkedList.h"
#include <iostream>
using namespace std;

LinkedList::LinkedList()
{
    head = nullptr;
    count = 0;
}

LinkedList::~LinkedList()
{
    clear();
}

void LinkedList::insert(int value)
{
    if (search(value))
    {
        return;                       // no duplicate paper IDs
    }

    ListNode* node = new ListNode;
    node->value = value;
    node->next = nullptr;

    if (head == nullptr)
    {
        head = node;                  // empty list: the new node becomes the head
    }
    else
    {
        ListNode* current = head;
        while (current->next != nullptr)
        {
            current = current->next;  // walk to the last node
        }
        current->next = node;
    }
    count++;
}

bool LinkedList::search(int value) const
{
    ListNode* current = head;
    while (current != nullptr)
    {
        if (current->value == value)
        {
            return true;
        }
        current = current->next;
    }
    return false;
}

void LinkedList::display() const
{
    ListNode* current = head;
    while (current != nullptr)
    {
        cout << current->value;
        if (current->next != nullptr)
        {
            cout << ", ";
        }
        current = current->next;
    }
    cout << endl;
}

void LinkedList::clear()
{
    while (head != nullptr)
    {
        ListNode* temp = head;
        head = head->next;
        delete temp;
    }
    count = 0;
}

int LinkedList::size() const
{
    return count;
}

ListNode* LinkedList::getHead() const
{
    return head;
}

// OR: every value of `other` is added (duplicates are skipped by insert).
void LinkedList::addAll(const LinkedList& other)
{
    ListNode* current = other.head;
    while (current != nullptr)
    {
        insert(current->value);
        current = current->next;
    }
}

// AND: values that are NOT in `other` are removed from this list.
void LinkedList::keepOnlyCommon(const LinkedList& other)
{
    ListNode* current = head;
    ListNode* previous = nullptr;

    while (current != nullptr)
    {
        if (!other.search(current->value))
        {
            ListNode* toDelete = current;
            if (previous == nullptr)
            {
                head = current->next;          // removing the first node
            }
            else
            {
                previous->next = current->next;
            }
            current = current->next;
            delete toDelete;
            count--;
        }
        else
        {
            previous = current;
            current = current->next;
        }
    }
}
