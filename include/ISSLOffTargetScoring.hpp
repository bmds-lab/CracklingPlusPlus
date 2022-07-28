
#pragma once
#include <cfdPenalties.h>
#include <unordered_map>
#include <unordered_set>
#include <phmap/phmap.h>
#include <ConfigManager.hpp>
#include <Constants.hpp>
#include <Helpers.hpp>

#ifdef _WIN32
#define portable_stat64 _stat64
#elif defined __linux__
#define portable_stat64 stat64
#endif

class ISSLOffTargetScoring
{
public:
	explicit ISSLOffTargetScoring(ConfigManager& cm);

	void run(std::map<std::string, std::map<std::string, std::string, std::less<>>, std::less<>>& candidateGuides);

private:
	bool toolIsSelected;
	std::string optimsationLevel;
	int toolCount;
	int consensusN;
	std::string offTargetScoreOutFile;
	std::string offTargetScoreInFile;
	std::string offTargetScoreBin;
	std::string offTargetScoreIndex;
	std::string offTargetScoreMaxDist;
	std::string offTargetScoreMethod;
	float offTagertScoreThreshold;
	int offTargetScorePageLength;

	size_t getFileSize(const char* path);

	uint64_t sequenceToSignature(const char* ptr, const size_t& seqLength);

	std::string signatureToSequence(uint64_t signature, const size_t& seqLength);
};