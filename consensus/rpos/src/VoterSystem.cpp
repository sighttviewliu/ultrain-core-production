#include "rpos/VoterSystem.h"

#include <limits>

#include <boost/math/distributions/binomial.hpp>

#include <crypto/Ed25519.h>
#include <rpos/Config.h>
#include <rpos/MessageManager.h>
#include <rpos/Node.h>
#include <rpos/Proof.h>

using boost::math::binomial;
using std::string;

namespace ultrainio {
    double VoterSystem::getProposerRatio() {
        long totalStake = getTotalStakes(UranusNode::getInstance()->getBlockNum());
        return Config::PROPOSER_STAKES_NUMBER / totalStake;
    }

    double VoterSystem::getVoterRatio() {
        long totalStake = getTotalStakes(UranusNode::getInstance()->getBlockNum());
        return Config::VOTER_STAKES_NUMBER / totalStake;
    }

    bool VoterSystem::isStillGenesis(uint32_t blockNum) const {
        std::shared_ptr<MessageManager> messageManagerPtr = MessageManager::getInstance();
        std::shared_ptr<std::vector<CommitteeInfo>> committeeInfoVPtr = messageManagerPtr->getCommitteeInfoVPtr(blockNum);
        //TODO(qinxiaofen) still genesis startup
        if (!committeeInfoVPtr || committeeInfoVPtr->size() < 6) {
            return true;
        }
        return false;
    }

    long VoterSystem::getTotalStakes(uint32_t blockNum) const {
        return getCommitteeMember(blockNum) * Config::DEFAULT_THRESHOLD;
    }

    int VoterSystem::getStakes(const std::string &pk) {
        std::shared_ptr<UranusNode> nodePtr = UranusNode::getInstance();
        bool isNonProducingNode = nodePtr->getNonProducingNode();
        uint32_t blockNum = nodePtr->getBlockNum();
        bool isGenesisLeader = nodePtr->isGenesisLeader(PublicKey(pk));
        string myPk(nodePtr->getPublicKey());
        // (shenyufeng)always be no listener
        if (isNonProducingNode && pk == myPk)
            return 0;
        if ((isStillGenesis(blockNum) && isGenesisLeader)
                || (!isStillGenesis(blockNum) && isCommitteeMember(PublicKey(pk)))) {
            return Config::DEFAULT_THRESHOLD;
        }
        return 0;
    }

    int VoterSystem::getCommitteeMember(uint32_t blockNum) const {
        if (isStillGenesis(blockNum)) {
            return 1; // genesis leader only
        }
        std::shared_ptr<MessageManager> messageManagerPtr = MessageManager::getInstance();
        std::shared_ptr<std::vector<CommitteeInfo>> committeeInfoVPtr = messageManagerPtr->getCommitteeInfoVPtr(blockNum);
        return committeeInfoVPtr->size();
    }

    bool VoterSystem::isCommitteeMember(const PublicKey& publicKey) const {
        // TODO(qinxiaofen) get from Committee member
        return true;
    }

    std::shared_ptr<std::vector<fc::variant>> VoterSystem::getCommitteeInfoList() {
        static const auto &ro_api = appbase::app().get_plugin<chain_plugin>().get_read_only_api();
        static struct chain_apis::read_only::get_producers_params params;
        params.json = false;
        params.lower_bound = "0";
        params.limit = 50;
        auto result = ro_api.get_producers(params, true);
//        auto result = rawResult.as<ultrainio::chain_apis::read_only::get_producers_result>();
        if (!result.rows.empty()) {
            for (const auto& r : result.rows ) {
               ilog("********************token ${token}, pk ${pk}", ("token", r)("pk", r["producer_key"]));
            }
        }
        return nullptr;
    }

    int VoterSystem::count(const Proof& proof, int stakes, double p) {
        double rand = proof.getRand();
        return reverseBinoCdf(rand, stakes, p);
    }

    int VoterSystem::reverseBinoCdf(double rand, int stake, double p) {
        int k = 0;
        binomial b(stake, p);

        while (rand > boost::math::cdf(b, k)) {
            k++;
        }
        return k;
    }
}
