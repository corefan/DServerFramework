#include "NetThreadSession.h"

MsgQueue<Net2LogicMsg>  gNet2LogicMsgList;
std::mutex              gNet2LogicMsgListLock;

void ExtNetSession::pushDataMsgToLogicThread(const char* data, size_t len)
{
    pushDataMsg2LogicMsgList(mLogicSession, data, len);
}

void ExtNetSession::onEnter()
{
    std::lock_guard<std::mutex> lck(gNet2LogicMsgListLock);
    mLogicSession->setSession(getServer(), getSocketID(), getIP());
    Net2LogicMsg tmp(mLogicSession, Net2LogicMsgTypeEnter);
    gNet2LogicMsgList.Push(tmp);
}

void ExtNetSession::onClose()
{
    std::lock_guard<std::mutex> lck(gNet2LogicMsgListLock);
    Net2LogicMsg tmp(mLogicSession, Net2LogicMsgTypeClose);
    gNet2LogicMsgList.Push(tmp);
}

void pushDataMsg2LogicMsgList(BaseLogicSession::PTR session, const char* data, size_t len)
{
    std::lock_guard<std::mutex> lck(gNet2LogicMsgListLock);
    Net2LogicMsg tmp(session, Net2LogicMsgTypeData);
    tmp.setData(data, len);
    gNet2LogicMsgList.Push(tmp);
}

void pushCompleteCallback2LogicMsgList(const DataSocket::PACKED_SENDED_CALLBACK& callback)
{
    std::lock_guard<std::mutex> lck(gNet2LogicMsgListLock);
    Net2LogicMsg tmp(nullptr, Net2LogicMsgTypeCompleteCallback);
    tmp.mSendCompleteCallback = callback;
    gNet2LogicMsgList.Push(tmp);
}

void syncNet2LogicMsgList(std::shared_ptr<EventLoop> eventLoop)
{
    gNet2LogicMsgListLock.lock();
    gNet2LogicMsgList.ForceSyncWrite();
    gNet2LogicMsgListLock.unlock();
    if (gNet2LogicMsgList.SharedListSize() > 0)
    {
        eventLoop->wakeup();
    }
}

void procNet2LogicMsgList()
{
    gNet2LogicMsgList.SyncRead(0);

    Net2LogicMsg msg;
    while (gNet2LogicMsgList.PopFront(msg))
    {
        switch (msg.mMsgType)
        {
            case Net2LogicMsgTypeEnter:
            {
                msg.mSession->onEnter();
            }
            break;
            case Net2LogicMsgTypeData:
            {
                msg.mSession->onMsg(msg.mPacket.c_str(), msg.mPacket.size());
            }
            break;
            case Net2LogicMsgTypeClose:
            {
                msg.mSession->onClose();
            }
            break;
            case Net2LogicMsgTypeCompleteCallback:
            {
                (*msg.mSendCompleteCallback)();
            }
            break;
            default:
                assert(false);
                break;
        }
    }
}