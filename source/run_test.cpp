#include <iostream>
#include <chrono>
#include <string>
#include <ctime>
#include <cmath>
#include <vector>
#include <random>
#include <array>
#include "enc_dec.h"
#include "ldpc.h"
#include "argmin.h"

const int N_TEST = 13;
const int N_ESNO = 2;

// Define structure for each test point, specifying code parameters and test conditions
struct test_point
{
  int k; // Number of information bits
  int n; // Number of codeword bits
  float esno[N_ESNO];  // Array of SNR values for testing
  int n_block[N_ESNO]; // Array of block sizes for testing
};

// Define set of tests
test_point contest[N_TEST] =
{
  {32,64,{5.0,10.0},{10,20}},    // Simple test parameters (k,n,esno_list,n_block_list)
  {64,256,{5.0,10.0},{10,20}},   // k=64 R=1/4
  {128,512,{5.0,10.0},{10,20}},  // k=128 R=1/4
  {256,1024,{5.0,10.0},{10,20}}, // k=256 R=1/4
  {512,2048,{5.0,10.0},{10,20}}, // k=512 R=1/4
  {64,128,{5.0,10.0},{10,20}},   // k=64 R=1/2
  {128,256,{5.0,10.0},{10,20}},  // k=128 R=1/2
  {256,512,{5.0,10.0},{10,20}},  // k=256 R=1/2
  {512,1024,{5.0,10.0},{10,20}}, // k=512 R=1/2
  {64,80,{5.0,10.0},{10,20}},    // k=64 R=4/5
  {128,160,{5.0,10.0},{10,20}},  // k=128 R=4/5
  {256,320,{5.0,10.0},{10,20}},  // k=256 R=4/5
  {512,640,{5.0,10.0},{10,20}}   // k=512 R=4/5
};

// Define class to collect statistics
template <typename T,int N> class stats
{
  protected:
    std::vector<std::array<T, N>> data; // Store data

  public:
    // clear
    void clear() { data.clear(); }

    // collect stats
    void add_sample(const std::array<T, N>& sample) { data.push_back(sample); }

    // return number of samples
    int n_sample() { return data.size(); }

    // return sum
    std::array<int, N> sum()
    {
      std::array<int, N> sum = {0};
      for (const auto& sample : data) {
          for (int i = 0; i < N; ++i) {
              sum[i] += sample[i];
          }
      }
      return sum;
    }
};
    
// Enumerate statistics to collect
enum dec_stat : int
{
  DET = 1,
  BIT = 2,
  ENCT = 3,
  DECT = 4
};

// Setup for decoder stats
class decoder_stats : public stats<int,4>
{
  public:
    void update(int det, int bit, int enc, int dec) {
        std::array<int, 4> dummy = {det, bit, enc, dec};
        stats<int, 4>::add_sample(dummy);
    }
    //std::vector<std::array<int, 4>> get_data() const { return data; }
};
    
// Simulate BPSK transmission over an AWGN channel
void channel(const bitvec& cw, float esno, llrvec& llr_out) {
    llr_out.resize(cw.size());
    std::default_random_engine generator;
    std::normal_distribution<float> distribution(0.0, std::sqrt(1.0 / (2.0 * esno)));

    for (size_t i = 0; i < cw.size(); ++i) {
        // BPSK modulation: 0 -> +1, 1 -> -1
        float modulated = (cw[i] == 0) ? 1.0f : -1.0f;
        // Add Gaussian noise
        llr_out[i] = modulated + distribution(generator);
    }
    }

// Run all the tests in one round
void run_test(int t, decoder_stats &stats)
{
  // Allocate variables
  test_point &test = contest[t];
  bitvec info(test.k);
  bitvec cw(test.n);
  llrvec llr(test.n);
  bitvec cw_est(test.n);
  bitvec info_est(test.n);

  // Setup binary RNG
  std::default_random_engine generator(static_cast<unsigned int>(time(0)));
  std::uniform_int_distribution<int> distribution(0, 1);

  // Construct encoder-decoder
  enc_dec entry;

  // Init encoder and decoder for entry
  if (entry.init(test.k,test.n) != 0) {
    // This submission does not handle this code
    std::cout << "Handle exception" << std::endl;
  }

  // Run tests
  stats.clear();
  for (int i = 0; i < test.n_block[0]; ++i)
  {
    // Generate random binary message of length test.k
    for (int j = 0; j < test.k; ++j) {
        info[j] = distribution(generator); // Random binary message
        info[j] = 0;
    }

    // Encode message
    auto enc_start = std::chrono::high_resolution_clock::now();
    entry.encode(info, cw);
    auto enc_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - enc_start).count();
 
    // Transmit message
    channel(cw, test.esno[0], llr);

    // Decode message
    auto dec_start = std::chrono::high_resolution_clock::now();
    int detect = entry.decode(llr, cw_est, &info_est);
    auto dec_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - dec_start).count();

    // Count number of bit errors
    int bit_err = 0;
    for (int j = 0; j < test.k; ++j) {
        if (info[j] != info_est[j]) {
            ++bit_err;
        }
    }

    // Update statistics
    stats.update(detect, bit_err, enc_time, dec_time);
  }
}


void run_single_test(int test_number) {
    srand(static_cast<unsigned int>(time(0))); // Seed random number generator
    decoder_stats run_stats;

    // Run the specified test and output results
    run_test(test_number, run_stats);
    int n_sample = run_stats.n_sample();
    auto sum = run_stats.sum();
    std::array<int, 4> mean;
    for (int i = 0; i < 4; ++i) {
        mean[i] = sum[i] / n_sample;
    }
    std::cout << "Test " << test_number << ": "
              << "Detect: " << sum[0] << "/" << n_sample << " = " << mean[0] << ", "
              << "Bit Errors: " << sum[1]  << "/" << n_sample*contest[test_number].k << " = " << mean[1] << ", "
              << "Encoding Time: " << sum[2]  << "/" << n_sample << " = " << mean[2] << ", "
              << "Decoding Time: " << sum[3]  << "/" << n_sample << " = " << mean[3] << ", " << std::endl;
}

// Setup option for argmin parsing
OptionSpec options[] = {
    {"-h", "--help",   false, "Show this help message"},
    {"-t", "--test",  true,  "Choose the test or use 'all'"},
    {nullptr, nullptr, false, nullptr} // sentinel to mark end
};

int main(int argc, char* argv[])
{
    // Parse options
    int argmin_result;
    std::map<std::string,std::string> parsedOptions;
    argmin_result = argmin(options, argc, argv, parsedOptions);
    switch (argmin_result) {
      case 1:
        return 1;
      case 2:
        return 0;
    }

    auto iter = parsedOptions.find("test");
    if (iter != parsedOptions.end()) {
        if (iter->second == "all") {
            for (int i = 0; i < N_TEST; ++i) {
                run_single_test(i);
            }
        } else {
            int test_number = std::stoi(iter->second);
            if (test_number >= 0 && test_number < N_TEST) {
                run_single_test(test_number);
            } else {
                std::cerr << "Invalid test number: " << test_number << std::endl;
            }
        }
    }

    // Success
    return 0;
}
