#pragma once

class CSession;
class Content;

class ContentManager {
    struct Entry {
        int contentID;
        Content* pContent;
    };

public:
    ContentManager(HANDLE port);

    void registerContent(const int contentID, Content* c);

    void transferSession(const int contentID, CSession* pSession);

    // 프레임마다 PQCS 호출
    void Tick(void);

private:
    HANDLE hIOCP;
    std::vector<Entry> entries;
};
