#pragma once

#include <string>
#include <vector>

#include <core/types.h>
#include <lightclient/CommitteeSet.h>

namespace ultrainio {
    class StartPoint {
    public:
        CommitteeSet committeeSet;
        BlockIdType lastConfirmedBlockId;
        std::string nextCommitteeMroot = std::string();
    };
}