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

    void registerContent(const int contentID, Content* c);  // 컨텐츠를 컨텐츠 매니저에 등록

    void transferSession(const int contentID, CSession* pSession);  // 세션을 다른 컨텐츠로 옮기는 기능

    void removeSession(CSession* pSession); // 세션의 삭제를 컨텐츠에 요청하는 기능

    // 프레임마다 PQCS 호출
    void Tick(void);

private:
    HANDLE hIOCP;
    std::vector<Entry> entries;
};
