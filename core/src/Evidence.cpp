#include "core/Evidence.h"

namespace ultrainio {
    const std::string Evidence::kType = "type";

    const int Evidence::kReporterEvil = -1;

    const int Evidence::kNone = 0x0;

    const int Evidence::kMultiPropose = 0x1; // send multi-propose message

    const int Evidence::kMultiVote = 0x2; // eg. ba0 vote multi-propose

    Evidence::~Evidence() {};

    bool Evidence::isNull() const {
        return false;
    }

    std::string Evidence::toString() const {
        return std::string();
    }

    AccountName Evidence::getEvilAccount() const {
        return AccountName();
    }

    int Evidence::verify(const AccountName& accountName, const PublicKey& pk) const {
        return Evidence::kNone;
    }

    bool Evidence::simpleVerify() const {
        return false;
    }
}
