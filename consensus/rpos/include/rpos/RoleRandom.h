#pragma once

#include <core/Redefined.h>

namespace ultrainio {
    class RoleRandom {
    public:
        RoleRandom(const BlockIdType& r);

        uint32_t toInt();

    private:
        BlockIdType m_rand;
    };
}