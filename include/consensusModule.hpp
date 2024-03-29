#ifndef consensusModuleInclude
#define consensusModuleInclude
#include <vector>
#include "../include/pipelineModule.hpp"
#include "../include/util.hpp"

class consensusModule : private pipelineModule
{
protected:
	bool toolIsSelected;
	optimisationLevel optimsationLevel;
	uint8_t toolCount;
	uint8_t consensusN;
	consensusModule(const cracklingConfig& config);
	virtual void run(std::vector<guideResults>& candidateGuides) = 0;
	virtual bool processGuide(const guideResults& guide) = 0;
private:
	 void run() final;
};

#endif // !consensusModuleInclude
