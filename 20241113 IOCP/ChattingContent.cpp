#include "pch.h"
#include "ChattingContent.h"

ChattingContent::ChattingContent(int id, ContentManager* mgr)
    : Content(id, mgr)
{
}

void ChattingContent::OnSessionEnter(CSession* s)
{
    //std::cout << "[Chat] Session " << s->id << " entered chat\n";
}

void ChattingContent::OnSessionLeave(CSession* s)
{
    //std::cout << "[Chat] Session " << s->id << " left chat\n";
}

void ChattingContent::Update()
{
    //std::cout << "Update È£ÃâµÊ!\n";
}

void ChattingContent::ProcessPacket(CPacket* p)
{
}
