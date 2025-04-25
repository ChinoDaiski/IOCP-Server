#pragma once

class CSession;
class Content;

class ContentManager {
    struct Entry {
        int contentID;
        Content* pContent;
    };

public:
    ContentManager(void);
    void InitWithIOCP(HANDLE port);

    void registerContent(const int contentID, Content* c);

    void transferSession(const int contentID, CSession* pSession);

    // �����Ӹ��� PQCS ȣ��
    void Tick(void);

private:
    HANDLE hIOCP;
    std::vector<Entry> entries;
};
