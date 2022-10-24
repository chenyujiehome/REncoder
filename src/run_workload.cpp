#include <bits/stdc++.h>
#include <immintrin.h>
#include <chrono>
#include "BOBHash32.h"
#include "RBF.h"
#include "REncoder.h"
using namespace std;

// For pretty print.
static const char *kGreen = "\033[0;32m";
static const char *kWhite = "\033[0;0m";

// Number of queries to RBF.
long long query_count = 0;
// Number of cache hits (the queried prefix is the same as the last queried one).
long long cache_hit = 0;

// Parameters of REncoder.
int BPK;
int HASH_NUM;
int STORED_LEVELS;
int IS_SELF_ADAPT;
int SELF_ADAPT_STEP;

RENCODER rencoder;

// Parameters of workload.
string KEY_TYPE;
int TOTAL_ITEM_NUM;
int RANGE_QUERY_NUM;

vector<uint64_t> keys;
vector<pair<uint64_t, uint64_t>> range_queries;
set<uint64_t> key_set;

// Convert string to unsigned 64-bit integer.
uint64_t stringToUINT64(const std::string s)
{
    uint64_t out = 0;
    memcpy(&out, s.c_str(), 8);
    out = __builtin_bswap64(out);
    return out;
}

// Load keys from file.
void LoadKey()
{
    ifstream keyFile;
    keyFile.open("../data/key.txt");
    uint64_t key;
    string keystring;
    for (int i = 0; i < TOTAL_ITEM_NUM; i++)
    {
        if (KEY_TYPE == "string")
        {
            keyFile >> keystring;
            key = stringToUINT64(keystring);
        }
        else
        {
            keyFile >> key;
        }
        keys[i] = key;
    }
    sort(keys.begin(), keys.end());
    for (int i = 0; i < TOTAL_ITEM_NUM; i++)
    {
        key_set.insert(keys[i]);
    }
    keyFile.close();
}

// Load range queries from file.
void LoadQuery()
{
    ifstream leftFile, rightFile;
    // Load the left and right boundaries of the range queries separately.
    leftFile.open("../data/lower_bound.txt");
    rightFile.open("../data/upper_bound.txt");
    uint64_t lower;
    string lowerstring;
    uint64_t upper;
    string upperstring;
    for (int i = 0; i < RANGE_QUERY_NUM; i++)
    {
        if (KEY_TYPE == "string")
        {
            leftFile >> lowerstring;
            lower = stringToUINT64(lowerstring);
            rightFile >> upperstring;
            upper = stringToUINT64(upperstring);
        }
        else
        {
            leftFile >> lower;
            rightFile >> upper;
        }
        upper -= 1;
        range_queries[i] = make_pair(lower, upper);
    }
    // Shuffle to make the queries random.
    random_shuffle(range_queries.begin(), range_queries.end());
    leftFile.close();
    rightFile.close();
}

// Calculate and print the proportion of empty queries in all queries.
void PrintEmptyRate()
{
    set<uint64_t>::iterator iter;
    int empty_cnt = 0;
    for (int i = 0; i < RANGE_QUERY_NUM; i++)
    {
        iter = key_set.lower_bound(range_queries[i].first);
        if (!(iter != key_set.end() && (*iter) <= range_queries[i].second))
        {
            empty_cnt++;
        }
    }
    printf("Empty Rate: %lf\n", empty_cnt / 1.0 / RANGE_QUERY_NUM);
}

// Run the workload and print the metrics.
void RunWorkload()
{
    // Number of repetitions.
    int repeat = 3;

    range_queries.clear();
    key_set.clear();

    LoadKey();
    LoadQuery();
    PrintEmptyRate();

    // Initialize REncoder.
    uint64_t memory = (uint64_t)TOTAL_ITEM_NUM * BPK;
    rencoder.init(memory, HASH_NUM, 64, STORED_LEVELS);

    cout << "Insert " << TOTAL_ITEM_NUM << " items" << endl;
    cout << "Hash Num: " << HASH_NUM << endl;
    cout << "Bits per Key: " << BPK << endl;

    // Insert keys into REncoder.
    if (IS_SELF_ADAPT)
    {
        // Self-adaptively choose the optimal number of stored levels.
        int true_level = rencoder.Insert_SelfAdapt(keys, SELF_ADAPT_STEP);
        cout << "Number of stored levels: " << true_level << endl;
    }
    else
    {
        // Choose the number of stored levels given by arguments.
        for (int i = 0; i < TOTAL_ITEM_NUM; i++)
        {
            rencoder.Insert(keys[i]);
        }
        cout << "Number of stored levels: " << STORED_LEVELS << endl;
    }

    // Get the size of REncoder.
    pair<uint8_t *, size_t> ser = rencoder.serialize();
    printf("%sREncoder size: %.2lf MB\n%s", kGreen, ser.second / 1.0 / 1024 / 1024, kWhite);

    // Get the throughput of REncoder.
    cache_hit = 0;
    query_count = 0;
    long long TEST_QUERY_NUM = 0;
    auto start = chrono::high_resolution_clock::now();
    int res = 0;
    for (int k = 1; k <= repeat; k++)
        for (uint32_t i = 0; i < RANGE_QUERY_NUM; i++)
        {
            uint64_t l = range_queries[i].first, r = range_queries[i].second;
            TEST_QUERY_NUM++;
            res ^= rencoder.RangeQuery(l, r);
        }
    auto end = chrono::high_resolution_clock::now();
    uint64_t duration = chrono::duration_cast<chrono::nanoseconds>(end - start).count();
    double throughput = (double)1000.0 * TEST_QUERY_NUM / duration;
    printf("%sFilter Throughput: %lf Mops/s\n%s", kGreen, throughput, kWhite);
    printf("query cnt: %lld; cache hit: %lld; hit rate: %lf\n", query_count / repeat, cache_hit / repeat, cache_hit / 1.0 / query_count);

    // Get the false positive rate of REncoder.
    double FP = 0, TOTAL = 0;
    for (uint32_t i = 0; i < RANGE_QUERY_NUM; i++)
    {
        uint64_t l = range_queries[i].first, r = range_queries[i].second;
        auto iter = key_set.lower_bound(range_queries[i].first);
        if (iter != key_set.end() && (*iter) <= range_queries[i].second)
        {
            // Test for false negatives.
            if (!rencoder.RangeQuery(l, r))
            {
                cout << "Range Query Error (False Negative)";
                exit(-1);
            }
            continue;
        }
        TOTAL++;
        if (rencoder.RangeQuery(l, r))
        {
            FP++;
        }
    }
    printf("%sFalse Positive Rate: %lf\n%s", kGreen, FP / TOTAL, kWhite);
}

int main(int argc, char *argv[])
{
    if (argc < 7)
    {
        cout << "error arg : <KEY_TYPE> <TOTAL_ITEM_NUM> <RANGE_QUERY_NUM> <BPK> <IS_SELF_ADAPT> <SELF_ADAPT_STEP/STORED_LEVELS>" << endl;
        return 0;
    }

    KEY_TYPE = argv[1];              // Type of keys.
    TOTAL_ITEM_NUM = atoi(argv[2]);  // Number of keys.
    RANGE_QUERY_NUM = atoi(argv[3]); // Number of range queries.
    BPK = atoi(argv[4]);             // Number of bits allocated for each key in REncoder.
    IS_SELF_ADAPT = atoi(argv[5]);   // Whether to insert keys self-adaptively.
    if (IS_SELF_ADAPT)
    {
        SELF_ADAPT_STEP = atoi(argv[6]); // Number of prefixes inserted for each key in each round.
    }
    else
    {
        STORED_LEVELS = atoi(argv[6]); // Number of stored levels given manually.
    }
    HASH_NUM = 3; // Number of hash functions used in Range Bloom Filter.
    keys.resize(TOTAL_ITEM_NUM);
    range_queries.resize(RANGE_QUERY_NUM);

    RunWorkload();

    return 0;
}