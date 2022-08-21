#pragma once
#include <memory>
#include <vector>
#include "channel.h"

class EventDispatcher {
public:
    EventDispatcher(){};
    virtual ~EventDispatcher(){};
    virtual void PollAdd(SP_Channel) = 0;
    virtual void PollMod(SP_Channel) = 0;
    virtual void PollDel(SP_Channel) = 0;
    virtual std::vector<SP_Channel> WaitForReadyChannels() = 0;
};

using SP_EventDispatcher = std::shared_ptr<EventDispatcher>;