#ifndef ISSLScoringModuleInclude
#define ISSLScoringModuleInclude
#include <atomic>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include <omp.h>
#include "../include/phmap/phmap.h"
#include "../include/libpopcnt/libpopcnt.h"
#include "../include/otScorePenalties.hpp"
#include "../include/specificityModule.hpp"
#include "../include/util.hpp"

class ISSLScoringModule : private specificityModule
{
public:
	ISSLScoringModule(const cracklingConfig& config);
	void run(std::vector<guideResults>& candidateGuides) final;
private:
	offTargetConfig config;
	std::filesystem::path ISSLIndex;
	bool calcMIT;
	bool calcCFD;
	uint64_t sequenceToSignature(const std::string& seq, uint64_t seqLen);
	std::string signatureToSequence(uint64_t sig, uint64_t seqLen);
	bool processGuide(const guideResults& guide) final;

};


#endif // !ISSLScoringModuleInclude
