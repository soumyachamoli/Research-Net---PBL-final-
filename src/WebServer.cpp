#include "WebServer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace std;

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET SocketType;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int SocketType;
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0     // Linux only: don't crash if the browser closes the connection early
#endif

const int REQUEST_SIZE = 8192;
const int RESPONSE_SIZE = 1000000;   // 1 MB, enough for every paper

// ------------------------------------------------------------ text helpers

// Adds text to the end of out (never writes past cap).
static void append(char* out, int& len, int cap, const char* text)
{
    for (int i = 0; text[i] != '\0' && len < cap - 1; i++)
    {
        out[len] = text[i];
        len++;
    }
    out[len] = '\0';
}

static void appendInt(char* out, int& len, int cap, int value)
{
    char number[20];
    snprintf(number, sizeof(number), "%d", value);
    append(out, len, cap, number);
}

static void appendFloat(char* out, int& len, int cap, float value)
{
    char number[32];
    snprintf(number, sizeof(number), "%.2f", value);     // 8.4 -> "8.40"
    append(out, len, cap, number);
}

// Adds "text" in JSON format: quotes around it, and " \ and line breaks escaped.
static void appendJsonString(char* out, int& len, int cap, const char* text)
{
    append(out, len, cap, "\"");
    for (int i = 0; text[i] != '\0'; i++)
    {
        char c = text[i];
        if (c == '"') append(out, len, cap, "\\\"");
        else if (c == '\\') append(out, len, cap, "\\\\");
        else if (c == '\n') append(out, len, cap, "\\n");
        else if ((unsigned char)c < 32) append(out, len, cap, " ");
        else
        {
            char one[2] = {c, '\0'};
            append(out, len, cap, one);
        }
    }
    append(out, len, cap, "\"");
}

// "deep+learning%20ai" -> "deep learning ai"  (changes the text in place)
static void urlDecode(char* text)
{
    int read = 0;
    int write = 0;
    while (text[read] != '\0')
    {
        if (text[read] == '+')
        {
            text[write] = ' ';
            read++;
        }
        else if (text[read] == '%' && text[read + 1] != '\0' && text[read + 2] != '\0')
        {
            char hex[3] = {text[read + 1], text[read + 2], '\0'};
            text[write] = (char)strtol(hex, nullptr, 16);
            read += 3;
        }
        else
        {
            text[write] = text[read];
            read++;
        }
        write++;
    }
    text[write] = '\0';
}

// Finds name=value in "q=deep+learning&id=3" and copies the decoded value.
static void getParam(const char* query, const char* name, char* value, int valueSize)
{
    value[0] = '\0';
    int nameLength = strlen(name);
    const char* p = query;
    while (*p != '\0')
    {
        if (strncmp(p, name, nameLength) == 0 && p[nameLength] == '=')
        {
            p += nameLength + 1;
            int i = 0;
            while (*p != '\0' && *p != '&' && i < valueSize - 1)
            {
                value[i] = *p;
                i++;
                p++;
            }
            value[i] = '\0';
            urlDecode(value);
            return;
        }
        while (*p != '\0' && *p != '&') p++;    // jump to the next parameter
        if (*p == '&') p++;
    }
}

static void appendPaper(char* out, int& len, int cap, const Paper& paper, bool withAbstract)
{
    append(out, len, cap, "{\"id\":");
    appendInt(out, len, cap, paper.id);
    append(out, len, cap, ",\"title\":");
    appendJsonString(out, len, cap, paper.title);
    append(out, len, cap, ",\"authors\":");
    appendJsonString(out, len, cap, paper.authors);
    append(out, len, cap, ",\"keywords\":");
    appendJsonString(out, len, cap, paper.keywords);
    append(out, len, cap, ",\"year\":");
    appendInt(out, len, cap, paper.year);
    append(out, len, cap, ",\"impactFactor\":");
    appendFloat(out, len, cap, paper.impactFactor);
    append(out, len, cap, ",\"quartile\":");
    appendJsonString(out, len, cap, paper.quartile);
    append(out, len, cap, ",\"journal\":");
    appendJsonString(out, len, cap, paper.journal);
    append(out, len, cap, ",\"url\":");
    appendJsonString(out, len, cap, paper.url);
    if (withAbstract)
    {
        append(out, len, cap, ",\"abstract\":");
        appendJsonString(out, len, cap, paper.abstractText);
    }
    append(out, len, cap, "}");
}

// ------------------------------------------------------------ sending

static void sendAll(SocketType client, const char* data, int length)
{
    int sent = 0;
    while (sent < length)
    {
        int n = send(client, data + sent, length - sent, MSG_NOSIGNAL);
        if (n <= 0) return;
        sent += n;
    }
}

static void sendResponse(SocketType client, int status, const char* contentType, const char* body, int bodyLength)
{
    char header[300];
    const char* statusText = status == 200 ? "OK" : (status == 404 ? "Not Found" : "Bad Request");
    int headerLength = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
        status, statusText, contentType, bodyLength);
    sendAll(client, header, headerLength);
    sendAll(client, body, bodyLength);
}

// Sends a file from the web/ folder ("/" means web/index.html).
static void sendFile(SocketType client, const char* path)
{
    if (strstr(path, "..") != nullptr)
    {
        sendResponse(client, 404, "text/plain", "Not found", 9);    // block ../ tricks
        return;
    }
    char filePath[300];
    if (strcmp(path, "/") == 0) snprintf(filePath, sizeof(filePath), "web/index.html");
    else snprintf(filePath, sizeof(filePath), "web%s", path);

    ifstream file(filePath, ios::binary);
    if (!file)
    {
        sendResponse(client, 404, "text/plain", "Not found", 9);
        return;
    }
    file.seekg(0, ios::end);
    int size = (int)file.tellg();
    file.seekg(0, ios::beg);
    char* content = new char[size + 1];
    file.read(content, size);

    const char* type = "text/plain";
    if (strstr(filePath, ".html")) type = "text/html; charset=utf-8";
    else if (strstr(filePath, ".css")) type = "text/css";
    else if (strstr(filePath, ".js")) type = "application/javascript";

    sendResponse(client, 200, type, content, size);
    delete[] content;
}

// ------------------------------------------------------------ API

static int handleApi(SearchEngine& engine, Database& db, const char* method, const char* path,
                     const char* query, char* out, int& len, int cap)
{
    char value[500];

    if (strcmp(path, "/api/stats") == 0)
    {
        append(out, len, cap, "{\"papers\":");
        appendInt(out, len, cap, engine.getPaperCount());
        append(out, len, cap, ",\"words\":");
        appendInt(out, len, cap, engine.getWordCount());
        append(out, len, cap, "}");
        return 200;
    }

    // /api/search?q=...&yearFrom=&yearTo=&author=&quartile=&minImpact=&sort=
    if (strcmp(path, "/api/search") == 0)
    {
        char text[500];
        getParam(query, "q", text, sizeof(text));

        SearchFilter filter;
        clearFilter(filter);
        getParam(query, "yearFrom", value, sizeof(value));
        filter.yearFrom = atoi(value);
        getParam(query, "yearTo", value, sizeof(value));
        filter.yearTo = atoi(value);
        getParam(query, "author", filter.author, sizeof(filter.author));
        getParam(query, "quartile", filter.quartile, sizeof(filter.quartile));
        getParam(query, "minImpact", value, sizeof(value));
        filter.minImpact = (float)atof(value);
        getParam(query, "sort", value, sizeof(value));
        filter.sortBy = atoi(value);

        int ids[200];
        int count = engine.searchPapers(text, filter, ids, 200);

        append(out, len, cap, "{\"count\":");
        appendInt(out, len, cap, count);
        append(out, len, cap, ",\"results\":[");
        for (int i = 0; i < count; i++)
        {
            if (i > 0) append(out, len, cap, ",");
            appendPaper(out, len, cap, *engine.findPaper(ids[i]), false);
        }
        append(out, len, cap, "]}");
        return 200;
    }

    if (strcmp(path, "/api/autocomplete") == 0)
    {
        getParam(query, "prefix", value, sizeof(value));
        char suggestions[8][PHRASE_SIZE];
        int count = engine.autocomplete(value, suggestions, 8);   // Trie

        append(out, len, cap, "[");
        for (int i = 0; i < count; i++)
        {
            if (i > 0) append(out, len, cap, ",");
            appendJsonString(out, len, cap, suggestions[i]);
        }
        append(out, len, cap, "]");
        return 200;
    }

    // Opening a paper: read it from PostgreSQL and add it to the history.
    if (strcmp(path, "/api/paper") == 0)
    {
        getParam(query, "id", value, sizeof(value));
        int id = atoi(value);
        Paper paper;
        if (id <= 0 || !db.searchPaperById(id, paper))
        {
            append(out, len, cap, "{\"error\":\"Paper not found\"}");
            return 404;
        }
        engine.getHistory().addHistory(id);            // linked list: newest first
        appendPaper(out, len, cap, paper, true);
        return 200;
    }

    if (strcmp(path, "/api/recommend") == 0)
    {
        getParam(query, "id", value, sizeof(value));
        Recommendation recs[6];
        int count = engine.recommend(atoi(value), recs, 6);

        append(out, len, cap, "[");
        for (int i = 0; i < count; i++)
        {
            const Paper* paper = engine.findPaper(recs[i].paperId);
            if (i > 0) append(out, len, cap, ",");
            append(out, len, cap, "{\"paper\":");
            appendPaper(out, len, cap, *paper, false);
            append(out, len, cap, ",\"score\":");
            appendInt(out, len, cap, recs[i].score);
            append(out, len, cap, ",\"commonKeywords\":");
            appendInt(out, len, cap, recs[i].commonKeywords);
            append(out, len, cap, ",\"commonAuthors\":");
            appendInt(out, len, cap, recs[i].commonAuthors);
            append(out, len, cap, "}");
        }
        append(out, len, cap, "]");
        return 200;
    }

    // GET    /api/history         -> list (newest first)
    // DELETE /api/history?id=7    -> delete one entry
    // DELETE /api/history         -> clear everything
    if (strcmp(path, "/api/history") == 0)
    {
        History& history = engine.getHistory();
        if (strcmp(method, "DELETE") == 0)
        {
            getParam(query, "id", value, sizeof(value));
            if (value[0] == '\0') history.clearHistory();
            else history.deleteHistoryItem(atoi(value));
            append(out, len, cap, "{\"ok\":true}");
            return 200;
        }

        append(out, len, cap, "[");
        bool first = true;
        HistoryNode* current = history.getHead();
        while (current != nullptr)
        {
            const Paper* paper = engine.findPaper(current->paperId);
            if (paper != nullptr)
            {
                if (!first) append(out, len, cap, ",");
                appendPaper(out, len, cap, *paper, false);
                first = false;
            }
            current = current->next;
        }
        append(out, len, cap, "]");
        return 200;
    }

    append(out, len, cap, "{\"error\":\"Unknown request\"}");
    return 404;
}

static void handleClient(SocketType client, SearchEngine& engine, Database& db)
{
    char request[REQUEST_SIZE];
    int received = recv(client, request, REQUEST_SIZE - 1, 0);
    if (received <= 0)
    {
        return;
    }
    request[received] = '\0';

    // First line looks like: GET /api/search?q=machine HTTP/1.1
    char method[10] = "";
    char target[2048] = "";
    sscanf(request, "%9s %2047s", method, target);

    char path[2048];
    char query[2048] = "";
    copyText(path, target, sizeof(path));
    char* questionMark = strchr(path, '?');
    if (questionMark != nullptr)
    {
        copyText(query, questionMark + 1, sizeof(query));
        *questionMark = '\0';              // path ends where the query starts
    }

    if (strncmp(path, "/api/", 5) == 0)
    {
        char* body = new char[RESPONSE_SIZE];
        int length = 0;
        body[0] = '\0';
        int status = handleApi(engine, db, method, path, query, body, length, RESPONSE_SIZE);
        sendResponse(client, status, "application/json; charset=utf-8", body, length);
        delete[] body;
    }
    else
    {
        sendFile(client, path);
    }
}

// ------------------------------------------------------------ server loop

WebServer::WebServer(SearchEngine& searchEngine, Database& database) : engine(searchEngine), db(database)
{
}

void WebServer::start(int port)
{
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    SocketType server = socket(AF_INET, SOCK_STREAM, 0);
    int option = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char*)&option, sizeof(option));

    sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // only this computer can connect
    address.sin_port = htons(port);

    if (bind(server, (sockaddr*)&address, sizeof(address)) < 0 || listen(server, 10) < 0)
    {
        cout << "Could not start the website on port " << port << ". Is it already running?" << endl;
        return;
    }
    cout << "Website running at http://localhost:" << port << "  (press Ctrl+C to stop)" << endl;

    while (true)
    {
        SocketType client = accept(server, nullptr, nullptr);

        // Don't wait forever for a browser that connects but sends nothing.
#ifdef _WIN32
        DWORD timeout = 2000;
#else
        timeval timeout;
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
#endif
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

        handleClient(client, engine, db);
#ifdef _WIN32
        closesocket(client);
#else
        close(client);
#endif
    }
}
