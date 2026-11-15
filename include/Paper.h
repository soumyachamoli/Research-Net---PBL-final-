#ifndef PAPER_H
#define PAPER_H

// Sizes of the text fields (one extra character for the '\0' at the end).
const int TITLE_SIZE = 301;
const int AUTHORS_SIZE = 501;
const int KEYWORDS_SIZE = 1001;
const int ABSTRACT_SIZE = 1501;
const int QUARTILE_SIZE = 3;      // "Q1" + '\0'
const int JOURNAL_SIZE = 301;
const int URL_SIZE = 301;

// One research paper, exactly like one row of the "papers" table in PostgreSQL.
struct Paper
{
    int id;
    char title[TITLE_SIZE];
    char authors[AUTHORS_SIZE];       // comma separated: "R. Sharma, A. Verma"
    char keywords[KEYWORDS_SIZE];     // comma separated: "machine learning, healthcare"
    char abstractText[ABSTRACT_SIZE];
    int year;
    float impactFactor;               // 0 = no impact factor (e.g. preprints, conferences)
    char quartile[QUARTILE_SIZE];     // "Q1".."Q4", or "" if unknown
    char journal[JOURNAL_SIZE];
    char url[URL_SIZE];
};

#endif
