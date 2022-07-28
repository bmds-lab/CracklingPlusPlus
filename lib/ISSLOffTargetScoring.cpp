#include <ISSLOffTargetScoring.hpp>

using std::string;
using std::map;
using std::vector;
using std::pair;
using std::unordered_map;
using std::unordered_set;

/** Char to binary encoding */
const vector<uint8_t> nucleotideIndex{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
const vector<char> signatureIndex{ 'A', 'C', 'G', 'T'};
const enum ScoreMethod { unknown = 0, mit = 1, cfd = 2, mitAndCfd = 3, mitOrCfd = 4, avgMitCfd = 5 };

ISSLOffTargetScoring::ISSLOffTargetScoring(ConfigManager& cm) :
    toolIsSelected(cm.getBool("offtargetscore", "enabled")),
    optimsationLevel(cm.getString("general", "optimisation")),
    toolCount(cm.getConsensusToolCount()),
    consensusN(cm.getInt("consensus", "n")),
    offTargetScoreOutFile(cm.getString("offtargetscore", "output")),
    offTargetScoreInFile(cm.getString("offtargetscore", "input")),
    offTargetScoreBin(cm.getString("offtargetscore", "binary")),
    offTargetScoreIndex(cm.getString("input", "offtarget-sites")),
    offTargetScoreMaxDist(cm.getString("offtargetscore", "max-distance")),
    offTargetScoreMethod(cm.getString("offtargetscore", "method")),
    offTagertScoreThreshold(cm.getFloat("offtargetscore", "score-threshold")),
    offTargetScorePageLength(cm.getInt("offtargetscore", "page-length"))
{}

void ISSLOffTargetScoring::run(map<string, map<string, string, std::less<>>, std::less<>>& candidateGuides)
{
    if (!toolIsSelected)
    {
        printer("Off-target scoring has been configured not to run. Skipping Off-target scoring");
        return;
    }

    printer("Loading ISSL Index.");

    size_t seqLength, seqCount, sliceWidth, sliceCount, offtargetsCount, scoresCount;

    /** The maximum number of mismatches */
    int maxDist = stoi(offTargetScoreMaxDist);

    /** The threshold used to exit scoring early */
    double threshold = offTagertScoreThreshold;

    /** Scoring methods. To exit early:
     *      - only CFD must drop below `threshold`
     *      - only MIT must drop below `threshold`
     *      - both CFD and MIT must drop below `threshold`
     *      - CFD or MIT must drop below `threshold`
     *      - the average of CFD and MIT must below `threshold`
     */
    string argScoreMethod = offTargetScoreMethod;
    ScoreMethod scoreMethod = ScoreMethod::unknown;
    bool calcCfd = false;
    bool calcMit = false;
    if (!argScoreMethod.compare("and")) {
        scoreMethod = ScoreMethod::mitAndCfd;
        calcCfd = true;
        calcMit = true;
    }
    else if (!argScoreMethod.compare("or")) {
        scoreMethod = ScoreMethod::mitOrCfd;
        calcCfd = true;
        calcMit = true;
    }
    else if (!argScoreMethod.compare("avg")) {
        scoreMethod = ScoreMethod::avgMitCfd;
        calcCfd = true;
        calcMit = true;
    }
    else if (!argScoreMethod.compare("mit")) {
        scoreMethod = ScoreMethod::mit;
        calcMit = true;
    }
    else if (!argScoreMethod.compare("cfd")) {
        scoreMethod = ScoreMethod::cfd;
        calcCfd = true;
    }

    /** Begin reading the binary encoded ISSL, structured as:
     *      - a header (6 items)
     *      - precalcuated local MIT scores
     *      - all binary-encoded off-target sites
     *      - slice list sizes
     *      - slice contents
     */
    FILE* fp = fopen(offTargetScoreIndex.c_str(), "rb");

    /** The index contains a fixed-sized header
     *      - the number of off-targets in the index
     *      - the length of an off-target
     *      -
     *      - chars per slice
     *      - the number of slices per sequence
     *      - the number of precalculated MIT scores
     */
    vector<size_t> slicelistHeader(6);

    if (fread(slicelistHeader.data(), sizeof(size_t), slicelistHeader.size(), fp) == 0) {
        throw std::runtime_error("Error reading index: header invalid\n");
    }

    offtargetsCount = slicelistHeader[0];
    seqLength = slicelistHeader[1];
    seqCount = slicelistHeader[2];
    sliceWidth = slicelistHeader[3];
    sliceCount = slicelistHeader[4];
    scoresCount = slicelistHeader[5];

    /** The maximum number of possibly slice identities
     *      4 chars per slice * each of A,T,C,G = limit of 16
     */
    size_t sliceLimit = 1 << sliceWidth;

    /** Read in the precalculated MIT scores
     *      - `mask` is a 2-bit encoding of mismatch positions
     *          For example,
     *              00 01 01 00 01  indicates mismatches in positions 1, 3 and 4
     *
     *      - `score` is the local MIT score for this mismatch combination
     */
    phmap::flat_hash_map<uint64_t, double> precalculatedScores;

    for (int i = 0; i < scoresCount; i++) {
        uint64_t mask = 0;
        double score = 0.0;
        fread(&mask, sizeof(uint64_t), 1, fp);
        fread(&score, sizeof(double), 1, fp);

        precalculatedScores.insert(pair<uint64_t, double>(mask, score));
    }

    /** Load in all of the off-target sites */
    vector<uint64_t> offtargets(offtargetsCount);
    if (fread(offtargets.data(), sizeof(uint64_t), offtargetsCount, fp) == 0) {
        throw std::runtime_error("Error reading index: loading off-target sequences failed\n");
    }

    /** Prevent assessing an off-target site for multiple slices
     *
     *      Create enough 1-bit "seen" flags for the off-targets
     *      We only want to score a candidate guide against an off-target once.
     *      The least-significant bit represents the first off-target
     *      0 0 0 1   0 1 0 0   would indicate that the 3rd and 5th off-target have been seen.
     *      The CHAR_BIT macro tells us how many bits are in a byte (C++ >= 8 bits per byte)
     */
    uint64_t numOfftargetToggles = (offtargetsCount / ((size_t)sizeof(uint64_t) * (size_t)CHAR_BIT)) + 1;

    /** The number of signatures embedded per slice
     *
     *      These counts are stored contiguously
     *
     */
    vector<size_t> allSlicelistSizes(sliceCount * sliceLimit);

    if (fread(allSlicelistSizes.data(), sizeof(size_t), allSlicelistSizes.size(), fp) == 0) {
        throw std::runtime_error("Error reading index: reading slice list sizes failed\n");
    }

    /** The contents of the slices
     *
     *      Stored contiguously
     *
     *      Each signature (64-bit) is structured as:
     *          <occurrences 32-bit><off-target-id 32-bit>
     */
    vector<uint64_t> allSignatures(seqCount * sliceCount);

    if (fread(allSignatures.data(), sizeof(uint64_t), allSignatures.size(), fp) == 0) {
        throw std::runtime_error("Error reading index: reading slice contents failed\n");
    }

    /** End reading the index */
    fclose(fp);

    /** Start constructing index in memory
     *
     *      To begin, reverse the contiguous storage of the slices,
     *         into the following:
     *
     *         + Slice 0 :
     *         |---- AAAA : <slice contents>
     *         |---- AAAC : <slice contents>
     *         |----  ...
     *         |
     *         + Slice 1 :
     *         |---- AAAA : <slice contents>
     *         |---- AAAC : <slice contents>
     *         |---- ...
     *         | ...
     */
    vector<vector<uint64_t*>> sliceLists(sliceCount, vector<uint64_t*>(sliceLimit));

    uint64_t* offset = allSignatures.data();
    for (size_t i = 0; i < sliceCount; i++) {
        for (size_t j = 0; j < sliceLimit; j++) {
            size_t idx = i * sliceLimit + j;
            sliceLists[i][j] = offset;
            offset += allSlicelistSizes[idx];
        }
    }


    printer("Beginning Off-target scoring.");
    int testedCount = 0;
    int failedCount = 0;
    int pgIdx = 1;
    int guidesInPage = 0;
    auto paginatorIterator = candidateGuides.begin();
    auto pageStart = candidateGuides.begin();
    auto pageEnd = candidateGuides.begin();

    // Outer loop deals with changing iterator start and end points (Pagination)
    while (pageEnd != candidateGuides.end())
    {
        if (offTargetScorePageLength > 0)
        {
            // Advance the pageEnd pointer
            std::advance(pageEnd, (std::min)((int)std::distance(pageEnd, candidateGuides.end()), offTargetScorePageLength));
            // Record page start
            pageStart = paginatorIterator;
            // Print page information
            printer(fmt::format("\tProcessing page {} ({} per page).", commaify(pgIdx), commaify(offTargetScorePageLength)));
        }
        else {
            // Process all guides at once
            pageEnd = candidateGuides.end();
        }


        printer("\tConstructing the Off-target scoring input.");


        // New intergrated method (No need for external file, simply write to char vector)
        vector<char> queryDataSet;
        queryDataSet.reserve(offTargetScorePageLength * 20);
        vector<char> pamDataSet;
        pamDataSet.reserve(offTargetScorePageLength * 3);

        guidesInPage = 0;
        while (paginatorIterator != pageEnd)
        {
            string target23 = paginatorIterator->first;
            // Run time filtering
            if (!filterCandidateGuides(paginatorIterator->second, MODULE_SPECIFICITY, optimsationLevel, consensusN, toolCount)) {
                // Advance page end for each filtered out guide
                if (pageEnd != candidateGuides.end())
                {
                    pageEnd++;
                }
                paginatorIterator++;
                continue;
            }

            for (char c : target23.substr(0, 20))
            {
                queryDataSet.push_back(c);
            }

            for (char c : target23.substr(20))
            {
                pamDataSet.push_back(c);
            }
            guidesInPage++;
            paginatorIterator++;
        }



        printer(fmt::format("\t\t{} guides in this page.", commaify(guidesInPage)));

        // Call scoring method
        size_t seqLineLength = seqLength;

        size_t queryCount = guidesInPage;
        vector<uint64_t> querySignatures(queryCount);
        vector<double> querySignatureMitScores(queryCount);
        vector<double> querySignatureCfdScores(queryCount);

        /** Binary encode query sequences */
        #pragma omp parallel
        {
        #pragma omp for
            for (size_t i = 0; i < queryCount; i++) {
                char* ptr = &queryDataSet[i * seqLineLength];
                uint64_t signature = sequenceToSignature(ptr, seqLength);
                querySignatures[i] = signature;
            }
        }

        /** Begin scoring */
        #pragma omp parallel
        {
            unordered_map<uint64_t, unordered_set<uint64_t>> searchResults;
            vector<uint64_t> offtargetToggles(numOfftargetToggles);

            uint64_t* offtargetTogglesTail = offtargetToggles.data() + numOfftargetToggles - 1;

            /** For each candidate guide */
        #pragma omp for
            for (size_t searchIdx = 0; searchIdx < querySignatures.size(); searchIdx++) {

                auto searchSignature = querySignatures[searchIdx];

                /** Global scores */
                double totScoreMit = 0.0;
                double totScoreCfd = 0.0;

                int numOffTargetSitesScored = 0;
                double maximum_sum = (10000.0 - threshold * 100) / threshold;
                bool checkNextSlice = true;

                /** For each ISSL slice */
                for (size_t i = 0; i < sliceCount; i++) {
                    uint64_t sliceMask = sliceLimit - 1;
                    int sliceShift = sliceWidth * i;
                    sliceMask = sliceMask << sliceShift;
                    auto& sliceList = sliceLists[i];

                    uint64_t searchSlice = (searchSignature & sliceMask) >> sliceShift;

                    size_t idx = i * sliceLimit + searchSlice;

                    size_t signaturesInSlice = allSlicelistSizes[idx];
                    uint64_t* sliceOffset = sliceList[searchSlice];

                    /** For each off-target signature in slice */
                    for (size_t j = 0; j < signaturesInSlice; j++) {

                        auto signatureWithOccurrencesAndId = sliceOffset[j];
                        auto signatureId = signatureWithOccurrencesAndId & 0xFFFFFFFFull;
                        uint32_t occurrences = (signatureWithOccurrencesAndId >> (32));

                        /** Find the positions of mismatches
                         *
                         *  Search signature (SS):    A  A  T  T    G  C  A  T
                         *                           00 00 11 11   10 01 00 11
                         *
                         *        Off-target (OT):    A  T  A  T    C  G  A  T
                         *                           00 11 00 11   01 10 00 11
                         *
                         *                SS ^ OT:   00 00 11 11   10 01 00 11
                         *                         ^ 00 11 00 11   01 10 00 11
                         *                  (XORd) = 00 11 11 00   11 11 00 00
                         *
                         *        XORd & evenBits:   00 11 11 00   11 11 00 00
                         *                         & 10 10 10 10   10 10 10 10
                         *                   (eX)  = 00 10 10 00   10 10 00 00
                         *
                         *         XORd & oddBits:   00 11 11 00   11 11 00 00
                         *                         & 01 01 01 01   01 01 01 01
                         *                   (oX)  = 00 01 01 00   01 01 00 00
                         *
                         *         (eX >> 1) | oX:   00 01 01 00   01 01 00 00 (>>1)
                         *                         | 00 01 01 00   01 01 00 00
                         *            mismatches   = 00 01 01 00   01 01 00 00
                         *
                         *   popcount(mismatches):   4
                         */
                        uint64_t xoredSignatures = searchSignature ^ offtargets[signatureId];
                        uint64_t evenBits = xoredSignatures & 0xAAAAAAAAAAAAAAAAull;
                        uint64_t oddBits = xoredSignatures & 0x5555555555555555ull;
                        uint64_t mismatches = (evenBits >> 1) | oddBits;
                        int dist = __popcnt64(mismatches);

                        if (dist >= 0 && dist <= maxDist) {

                            /** Prevent assessing the same off-target for multiple slices */
                            uint64_t seenOfftargetAlready = 0;
                            uint64_t* ptrOfftargetFlag = (offtargetTogglesTail - (signatureId / 64));
                            seenOfftargetAlready = (*ptrOfftargetFlag >> (signatureId % 64)) & 1ULL;


                            if (!seenOfftargetAlready) {
                                // Begin calculating MIT score
                                if (calcMit) {
                                    if (dist > 0) {
                                        totScoreMit += precalculatedScores[mismatches] * (double)occurrences;
                                    }
                                }

                                // Begin calculating CFD score
                                if (calcCfd) {
                                    /** "In other words, for the CFD score, a value of 0
                                     *      indicates no predicted off-target activity whereas
                                     *      a value of 1 indicates a perfect match"
                                     *      John Doench, 2016.
                                     *      https://www.nature.com/articles/nbt.3437
                                    */
                                    double cfdScore = 0;
                                    if (dist == 0) {
                                        cfdScore = 1;
                                    }
                                    else if (dist > 0 && dist <= maxDist) {
                                        cfdScore = cfdPamPenalties[0b1010]; // PAM: NGG, TODO: do not hard-code the PAM

                                        for (size_t pos = 0; pos < 20; pos++) {
                                            size_t mask = pos << 4;

                                            /** Create the mask to look up the position-identity score
                                             *      In Python... c2b is char to bit
                                             *       mask = pos << 4
                                             *       mask |= c2b[sgRNA[pos]] << 2
                                             *       mask |= c2b[revcom(offTaret[pos])]
                                             *
                                             *      Find identity at `pos` for search signature
                                             *      example: find identity in pos=2
                                             *       Recall ISSL is inverted, hence:
                                             *                   3'-  T  G  C  C  G  A -5'
                                             *       start           11 10 01 01 10 00
                                             *       3UL << pos*2    00 00 00 11 00 00
                                             *       and             00 00 00 01 00 00
                                             *       shift           00 00 00 00 01 00
                                             */
                                            uint64_t searchSigIdentityPos = searchSignature;
                                            searchSigIdentityPos &= (3UL << (pos * 2));
                                            searchSigIdentityPos = searchSigIdentityPos >> (pos * 2);
                                            searchSigIdentityPos = searchSigIdentityPos << 2;

                                            /** Find identity at `pos` for offtarget
                                             *      Example: find identity in pos=2
                                             *      Recall ISSL is inverted, hence:
                                             *                  3'-  T  G  C  C  G  A -5'
                                             *      start           11 10 01 01 10 00
                                             *      3UL<<pos*2      00 00 00 11 00 00
                                             *      and             00 00 00 01 00 00
                                             *      shift           00 00 00 00 00 01
                                             *      rev comp 3UL    00 00 00 00 00 10 (done below)
                                             */
                                            uint64_t offtargetIdentityPos = offtargets[signatureId];
                                            offtargetIdentityPos &= (3UL << (pos * 2));
                                            offtargetIdentityPos = offtargetIdentityPos >> (pos * 2);

                                            /** Complete the mask
                                             *      reverse complement (^3UL) `offtargetIdentityPos` here
                                             */
                                            mask = (mask | searchSigIdentityPos | (offtargetIdentityPos ^ 3UL));

                                            if (searchSigIdentityPos >> 2 != offtargetIdentityPos) {
                                                cfdScore *= cfdPosPenalties[mask];
                                            }
                                        }
                                    }
                                    totScoreCfd += cfdScore * (double)occurrences;
                                }

                                *ptrOfftargetFlag |= (1ULL << (signatureId % 64));
                                numOffTargetSitesScored += occurrences;

                                /** Stop calculating global score early if possible */
                                if (scoreMethod == ScoreMethod::mitAndCfd) {
                                    if (totScoreMit > maximum_sum && totScoreCfd > maximum_sum) {
                                        checkNextSlice = false;
                                        break;
                                    }
                                }
                                if (scoreMethod == ScoreMethod::mitOrCfd) {
                                    if (totScoreMit > maximum_sum || totScoreCfd > maximum_sum) {
                                        checkNextSlice = false;
                                        break;
                                    }
                                }
                                if (scoreMethod == ScoreMethod::avgMitCfd) {
                                    if (((totScoreMit + totScoreCfd) / 2.0) > maximum_sum) {
                                        checkNextSlice = false;
                                        break;
                                    }
                                }
                                if (scoreMethod == ScoreMethod::mit) {
                                    if (totScoreMit > maximum_sum) {
                                        checkNextSlice = false;
                                        break;
                                    }
                                }
                                if (scoreMethod == ScoreMethod::cfd) {
                                    if (totScoreCfd > maximum_sum) {
                                        checkNextSlice = false;
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    if (!checkNextSlice)
                        break;
                }

                querySignatureMitScores[searchIdx] = 10000.0 / (100.0 + totScoreMit);
                querySignatureCfdScores[searchIdx] = 10000.0 / (100.0 + totScoreCfd);

                memset(offtargetToggles.data(), 0, sizeof(uint64_t) * offtargetToggles.size());
            }

        }

        printer("\tStarting to process the Off-target scoring results.");

        for (size_t searchIdx = 0; searchIdx < querySignatures.size(); searchIdx++) {
            string target20 = signatureToSequence(querySignatures[searchIdx], seqLength);
            char* pamPtr = &queryDataSet[searchIdx * 3];
            string pam(pamPtr, pamPtr + 3);
            string target23 = target20 + pam;
            candidateGuides[target23]["mitOfftargetscore"] = calcMit ? std::to_string(querySignatureMitScores[searchIdx]) : "\t-1";
            candidateGuides[target23]["cfdOfftargetscore"] = calcCfd ? std::to_string(querySignatureCfdScores[searchIdx]) : "-1\n";

            if (offTargetScoreMethod == "mit")
            {
                if (std::stof(candidateGuides[target23]["mitOfftargetscore"]) < offTagertScoreThreshold)
                {
                    candidateGuides[target23]["passedOffTargetScore"] = CODE_REJECTED;
                    failedCount++;
                }
                else { candidateGuides[target23]["passedOffTargetScore"] = CODE_ACCEPTED; }
            }

            else if (offTargetScoreMethod == "cfd")
            {
                if (std::stof(candidateGuides[target23]["cfdOfftargetscore"]) < offTagertScoreThreshold)
                {
                    candidateGuides[target23]["passedOffTargetScore"] = CODE_REJECTED;
                    failedCount++;
                }
                else { candidateGuides[target23]["passedOffTargetScore"] = CODE_ACCEPTED; }
            }

            else if (offTargetScoreMethod == "and")
            {
                if ((std::stof(candidateGuides[target23]["mitOfftargetscore"]) < offTagertScoreThreshold) && (std::stof(candidateGuides[target23]["cfdOfftargetscore"]) < offTagertScoreThreshold))
                {
                    candidateGuides[target23]["passedOffTargetScore"] = CODE_REJECTED;
                    failedCount++;
                }
                else { candidateGuides[target23]["passedOffTargetScore"] = CODE_ACCEPTED; }
            }

            else if (offTargetScoreMethod == "or")
            {
                if ((std::stof(candidateGuides[target23]["mitOfftargetscore"]) < offTagertScoreThreshold) || (std::stof(candidateGuides[target23]["cfdOfftargetscore"]) < offTagertScoreThreshold))
                {
                    candidateGuides[target23]["passedOffTargetScore"] = CODE_REJECTED;
                    failedCount++;
                }
                else { candidateGuides[target23]["passedOffTargetScore"] = CODE_ACCEPTED; }
            }

            else if (offTargetScoreMethod == "avg")
            {
                if (((std::stof(candidateGuides[target23]["mitOfftargetscore"]) + std::stof(candidateGuides[target23]["cfdOfftargetscore"])) / 2) < offTagertScoreThreshold)
                {
                    candidateGuides[target23]["passedOffTargetScore"] = CODE_REJECTED;
                    failedCount++;
                }
                else { candidateGuides[target23]["passedOffTargetScore"] = CODE_ACCEPTED; }
            }
            testedCount++;
        }

        printer(fmt::format("\t{} of {} failed here.", commaify(failedCount), commaify(testedCount)));
    }

}

/// Returns the size (bytes) of the file at `path`
size_t ISSLOffTargetScoring::getFileSize(const char* path)
{
    struct portable_stat64 statBuf;
    portable_stat64(path, &statBuf);
    return statBuf.st_size;
}

/**
 * Binary encode genetic string `ptr`
 *
 * For example,
 *   ATCG becomes
 *   00 11 01 10  (buffer with leading zeroes to encode as 64-bit unsigned int)
 *
 * @param[in] ptr the string containing ATCG to binary encode
 */
uint64_t ISSLOffTargetScoring::sequenceToSignature(const char* ptr, const size_t& seqLength)
{
    uint64_t signature = 0;
    for (size_t j = 0; j < seqLength; j++) {
        signature |= (uint64_t)(nucleotideIndex[*ptr]) << (j * 2);
        ptr++;
    }
    return signature;
}

/**
 * Binary encode genetic string `ptr`
 *
 * For example,
 *   00 11 01 10 becomes (as 64-bit unsigned int)
 *    A  T  C  G  (without spaces)
 *
 * @param[in] signature the binary encoded genetic string
 */
string ISSLOffTargetScoring::signatureToSequence(uint64_t signature, const size_t& seqLength)
{
    string sequence = string(seqLength, ' ');
    for (size_t j = 0; j < seqLength; j++) {
        sequence[j] = signatureIndex[(signature >> (j * 2)) & 0x3];
    }
    return sequence;
}