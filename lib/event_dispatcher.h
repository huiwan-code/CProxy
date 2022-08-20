#include "channel.h"

class EventDispatcher {
public:
    virtual void PollAdd(SP_Channel) = 0;
    virtual void PollMod(SP_Channel) = 0;
    virtual void PollDel(SP_Channel) = 0;
    virtual std::vector<SP_Channel> WaitForReadyChannels() = 0;
};