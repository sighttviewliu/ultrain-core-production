#include "rpos/EvidenceFactory.h"

#include <fc/variant.hpp>
#include <fc/io/json.hpp>

#include "rpos/EvidenceMultiSign.h"
#include "rpos/EvidenceNull.h"

namespace ultrainio {
    std::shared_ptr<Evidence> EvidenceFactory::create(const std::string& str) {
        fc::variant v = fc::json::from_string(str);
        if (v.is_object()) {
            fc::variant_object o = v.get_object();
            fc::variant typeVar = o[Evidence::kType];
            if (typeVar.as_int64() == Evidence::kSignMultiPropose) {
                return std::make_shared<EvidenceMultiSign>(str);
            }
        }
        return std::make_shared<EvidenceNull>();
    }
}