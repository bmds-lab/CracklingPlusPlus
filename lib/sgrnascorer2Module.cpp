#include "../include/sgrnascorer2Module.hpp"

using std::cout;
using std::endl;
using std::string;
using std::unordered_map;
using std::array;

const unordered_map <char, string> encoding = {
	{'A' , "0001"},
	{'C' , "0010"},
	{'T' , "0100"},
	{'G' , "1000"},
	{'K' , "1100"},
	{'M' , "0011"},
	{'R' , "1001"},
	{'Y' , "0110"},
	{'S' , "1010"},
	{'W' , "0101"},
	{'B' , "1110"},
	{'V' , "1011"},
	{'H' , "0111"},
	{'D' , "1101"},
	{'N' , "1111"},
};

sgrnascorer2Module::sgrnascorer2Module(const cracklingConfig& config) : consensusModule(config)
{
	this->toolIsSelected = config.consensus.sgrnascorer2;
	this->config = config.sgrnascorer2;
}

void sgrnascorer2Module::run(std::vector<guideResults>& candidateGuides)
{

	if (!toolIsSelected)
	{
		cout << "sgRNAScorer2 has been configured not to run. Skipping sgRNAScorer2" << endl;
		return;
	}

	struct svm_model* sgRNAScorer2Model = svm_load_model(config.model.string().c_str());

	cout << "sgRNAScorer2 - score using model." << endl;
	uint64_t failedCount = 0;
	uint64_t testedCount = 0;
	for (guideResults& candidate : candidateGuides)
	{

		// Run time filtering
		if (!processGuide(candidate)) { continue; }

		array<double, 80> encodedSeq;
		for (int i = 0; i < 20; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				encodedSeq[(i * 4) + j] = (double)encoding.at(candidate.seq.at(i)).at(j) - 48;
			}
		}

		auto nodeToTest = std::make_unique<array<svm_node, encodedSeq.size() + 1>>();
		for (int k = 0; k < encodedSeq.size(); k++)
		{
			(*nodeToTest.get())[k].index = k + 1; //index of value
			(*nodeToTest.get())[k].value = encodedSeq[k]; //value

		}
		(*nodeToTest.get())[encodedSeq.size()].index = -1;//state the end of data vector

		svm_predict_values(sgRNAScorer2Model, (const struct svm_node*)nodeToTest.get(), &candidate.sgrnascorer2score);

		if (candidate.sgrnascorer2score < config.scoreThreshold)
		{
			candidate.acceptedBySgRnaScorer2 = CODE_REJECTED;
			failedCount++;
		}
		else
		{
			candidate.acceptedBySgRnaScorer2 = CODE_ACCEPTED;
			candidate.consensusCount++;
		}
		testedCount++;
	}
	cout << fmt::format(comma_locale, "\t{:L} of {:L} failed here.", failedCount, testedCount) << endl;;

	svm_free_and_destroy_model(&sgRNAScorer2Model);
	return;

}

bool sgrnascorer2Module::processGuide(const guideResults& guide)
{
	// Process all guides at this level
	if (optimsationLevel == optimisationLevel::ultralow) { return true; }

	// For all levels above `ultralow`
	if (!guide.isUnique)
	{
		// Reject all guides that have been seen more than once
		return false;
	}

	// For optimisation level `high`
	if (optimsationLevel = optimisationLevel::high)
	{
		int countAlreadyAccepted =
			static_cast<int>(guide.passedG20 == CODE_ACCEPTED) +
			static_cast<int>(guide.acceptedByMm10db == CODE_ACCEPTED) +
			static_cast<int>(guide.acceptedBySgRnaScorer2 == CODE_ACCEPTED);

		int countAlreadyAssessed =
			static_cast<int>(guide.passedG20 != CODE_UNTESTED) +
			static_cast<int>(guide.acceptedByMm10db != CODE_UNTESTED) +
			static_cast<int>(guide.acceptedBySgRnaScorer2 != CODE_UNTESTED);

		// Reject if the consensus has already been passed
		if (countAlreadyAccepted >= consensusN) { return false; }

		// Reject if there is not enough tests remaining to pass consensus
		if (toolCount - countAlreadyAssessed < consensusN - countAlreadyAccepted) { return false; }
	}

	// None of the failure conditions have been meet, return true
	return true;
}
