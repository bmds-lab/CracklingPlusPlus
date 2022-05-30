#include <Helpers.hpp>
#include <doctest.h>

TEST_CASE("Reverse Compliments are computed correctly")
{
    SUBCASE("Test rc on palindromic seq")
    {
        CHECK( rc("ACGTACGTACGTACGTACGT") == "ACGTACGTACGTACGTACGT");
    }
    SUBCASE("Test rc on 'AT' seq")
    {
        CHECK( rc("ATATATATATATATATATATAGG") == "CCTATATATATATATATATATAT");
    }
    SUBCASE("Test rc on 'A' seq")
    {
        CHECK( rc("AAAAAAAAAAAAAAAAAAAAAGG") == "CCTTTTTTTTTTTTTTTTTTTTT");
    }
    SUBCASE("Test rc on 'T' seq")
    {
        CHECK( rc("TTTTTTTTTTTTTTTTTTTTTGG") == "CCAAAAAAAAAAAAAAAAAAAAA");
    }
    SUBCASE("Test rc on 'G' seq")
    {
        CHECK( rc("GGGGGGGGGGGGGGGGGGGGGGG") == "CCCCCCCCCCCCCCCCCCCCCCC");
    }
    SUBCASE("Test rc on 'C' seq")
    {
        CHECK( rc("CCCCCCCCCCCCCCCCCCCCCGG") == "CCGGGGGGGGGGGGGGGGGGGGG");
    }
    SUBCASE("Test rc on short seq ( < 23)")
    {
        CHECK( rc("GACTGG") == "CCAGTC");
    }
    SUBCASE("Test rc on long seq ( > 23)")
    {
        CHECK( rc("GACTGGTTTGTGTCATATTCTTCCTGTGG") == "CCACAGGAAGAATATGACACAAACCAGTC");
    }    
    SUBCASE("Test rc on empty seq")
    {
        CHECK_THROWS_WITH_AS(rc(""), "Type Error, Seqeunce length must be greater than 0!", std::length_error);
    }
    SUBCASE("Test rc on extremely large seq")
    {
        std::string extremelyLargeInput = "AAATCTAGCTGCTACGATCGATCGATCGCTAGCATGATCGATCGATCGATCGATCGACTGACTAGCTAGCATGCTAGCATCGATCGATGCTAGCATGCATGCTAGCATGCTAGCTAGCTAGCTAGCTAGCTAGCTAGCTAGCTGATCGATGACTGCTAGTACGTCGTACGTAGTCGTAGTAGCTGATCGATGCTACGTAGCATGCTAGCATGCATAGCTAGCTAGCTAGCTAGCTGCATAGCTAGCTAGCTAGCTGATCGATCGATCGATCGTCGCTAGAATCTAGCTGCTACGATCGATCGATCGCTAGCATGATCGATCGATCGATCGATCGACTGACTAGCTAGCATGCTAGCATCGATCGATGCTAGCATGCATGCTAGCATGCTAGCTAGCTAGCTAGCTAGCTAGCTAGCTAGCTGATCGATGACTGCTAGTACGTCGTACGTAGTCGTAGTAGCTGATCGATGCTACGTAGCATGCTAGCATGCATAGCTAGCTAGCTAGCTAGCTGCATAGCTAGCTAGCTAGCTGATCGATCGATCGATCGTCGCTAGAATCTAGCTGCTACGATCGATCGATCGCTAGCATGATCGATCGATCGATCGATCGACTGACTAGCTAGCATGCTAGCATCGATCGATGCTAGCATGCATGCTAGCATGCTAGCTAGCTAGCTAGCTAGCTAGCTAGCTAGCTGATCGATGACTGCTAGTACGTCGTACGTAGTCGTAGTAGCTGATCGATGCTACGTAGCATGCTAGCATGCATAGCTAGCTAGCTAGCTAGCTGCATAGCTAGCTAGCTAGCTGATCGATCGATCGATCGTCGCTAGAATCTAGCTGCTACGATCGATCGATCGCTAGCATGATCGATCGATCGATCGATCGACTGACTAGCTAGCATGCTAGCATCGATCGATGCTAGCATGCATGCTAGCATGCTAGCTAGCTAGCTAGCTAGCTAGCTAGCTAGCTGATCGATGACTGCTAGTACGTCGTACGTAGTCGTAGTAGCTGATCGAT";
        CHECK_THROWS_WITH_AS(rc(extremelyLargeInput), "Type Error, Seqeunce length must be less than 1024!", std::length_error);
    }
}