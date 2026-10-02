#include <iostream>
#include <sys/resource.h>
#include <chrono>
#include <string>
#include <ctime>
#include <cmath>
#include <vector>
#include <random>
#include <array>
#include <fstream>
#include <sstream>
#include "enc_dec.h"
#include "argmin.h"

const int N_TEST = 12;

// Global random number generator
std::mt19937 global_rng;

// Define structure for each test point, specifying code parameters and test conditions
struct test_point
{
  int k; // Number of information bits
  int n; // Number of codeword bits
  float esno;  // Array of SNR values for testing
  int n_block; // Array of block sizes for testing
  int opt_avg; // Flag to optimize average (versus max) decoding latency
};

// Define set of tests
test_point contest[N_TEST] =
{
  {64,256,1.0,2000,0},   // k=64 R=1/4
  {128,512,0.1,2000,0},  // k=128 R=1/4
  {256,1024,0.1,2000,0}, // k=256 R=1/4
  {512,2048,0.1,2000,0}, // k=512 R=1/4
  {64,128,1.0,2000,0},   // k=64 R=1/2
  {128,256,1.0,2000,0},  // k=128 R=1/2
  {256,512,1.0,2000,0},  // k=256 R=1/2
  {512,1024,1.0,2000,0}, // k=512 R=1/2
  {64,80,3.0,2000,0},    // k=64 R=4/5
  {128,160,3.0,2000,0},  // k=128 R=4/5
  {256,320,3.0,2000,0},  // k=256 R=4/5
  {512,640,3.0,2000,0}   // k=512 R=4/5
};

// Global defaults
float default_esno = 0.0;
int default_nblock = 0;
bool has_default_esno = false;
bool has_default_nblock = false;
unsigned int rng_seed = 0; // 0 means use clock-based seed

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

    // return sum (same type as stored values)
    std::array<T, N> sum()
    {
      std::array<T, N> sum{};           // all-zeros
      for (const auto& sample : data) {
          for (int i = 0; i < N; ++i) {
              sum[i] += sample[i];
          }
      }
      return sum;
    }

    // stream output
    void print(std::ostream* sout)
    {
      for (const auto& sample : data) {
          for (int i = 0; i < N; ++i) {
              *sout << sample[i] << " ";
          }
          *sout << std::endl;
      }
    }
};
    
// Enumerate statistics to collect
enum dec_stat : int
{
  BLK = 1,
  BIT = 2,
  ENCT = 3,
  DECT = 4
};

// Setup for decoder stats
class decoder_stats : public stats<long long,4>
{
  public:
    typedef std::array<long long, 4> statvec;
    void update(long long blk, long long bit,
                long long enc, long long dec) {
        statvec dummy = {blk, bit, enc, dec};
        stats<long long, 4>::add_sample(dummy);
    }
    //std::vector<std::array<int, 4>> get_data() const { return data; }
};
    
// Simulate BPSK transmission over an AWGN channel
void channel(const bitvec& cw, float esno, fltvec& llr_out) {
    llr_out.resize(cw.size());
    std::normal_distribution<float> distribution(4*esno, std::sqrt(8*esno));

    for (size_t i = 0; i < cw.size(); ++i) {
        // BPSK modulation: 0 -> +1, 1 -> -1
        float modulated = (cw[i] == 0) ? 1.0f : -1.0f;
        // Add Gaussian noise
        llr_out[i] = modulated * distribution(global_rng);
    }
}

// Run all the tests in one round
void run_test(int k, int n, float esno, int n_block, int opt_avg, decoder_stats &stats)
{
  stats.clear();
  if (k <= 0 || n <= k || esno < 0.0f || n_block <= 0) {
    std::cerr << "Invalid test parameters." << std::endl;
    return;
  }
  // Reset global RNG to ensure repeatability for each test
  if (rng_seed == 0) {
      rng_seed = std::chrono::system_clock::now().time_since_epoch().count();
  }
  global_rng.seed(rng_seed);

  // Allocate variables
  bitvec info(k);
  bitvec cw(n);
  fltvec float_llr(n);
  llrvec llr(n);
  bitvec cw_est(n);
  bitvec info_est(n);

  // Setup binary RNG
  std::uniform_int_distribution<int> distribution(0, 1);

  // Construct encoder-decoder
  enc_dec entry;

  // Init encoder and decoder for entry
  if (entry.init(k,n,opt_avg) != 0) {
    std::cerr << "Unable to initialize encoder-decoder." << std::endl;
    return;
  }

  // Run tests
  for (int i = 0; i < n_block; ++i)
  {
    // Generate random binary message of length test.k
    for (int j = 0; j < k; ++j) {
        info[j] = distribution(global_rng); // Random binary message
        //std::cout << info[j] << " ";
    }
    //std::cout << std::endl;

    // Encode message
    long long enc_time;
    do {
      auto enc_start = std::chrono::high_resolution_clock::now();
      entry.encode(info, cw);
      enc_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - enc_start).count();
    } while (enc_time < 0);

    // Transmit message
    channel(cw, esno, float_llr);

    // Convert int llr format
    for (int j = 0; j < n; ++j) llr[j] = entry.llr2int(float_llr[j]);

    // Decode message
    long long dec_time;
    do {
      auto dec_start = std::chrono::high_resolution_clock::now();
      //int detect = entry.decode(llr, cw_est, info_est);
      entry.decode(llr, cw_est, info_est);
      dec_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - dec_start).count();
    } while (dec_time < 0);

    // Count number of information bit errors
    int bit_err = 0;
    for (int j = 0; j < k; ++j) {
        //std::cout << info_est[j] << " ";
        if (info[j] != info_est[j]) {
            ++bit_err;
        }
    }

    // Update statistics
    stats.update(bit_err > 0, bit_err, enc_time, dec_time);
  }
}

// Run all the tests in one round
void run_test_number(int t, decoder_stats &stats)
{
  test_point &test = contest[t];
  float esno = test.esno;
  int n_block = test.n_block;
  if (has_default_esno) esno = default_esno;
  if (has_default_nblock) n_block = default_nblock;

  run_test(test.k, test.n, esno, n_block, test.opt_avg, stats);
}


void run_single_test(int test_number) {
    decoder_stats run_stats;
    run_stats.clear();

    // Setup the specified test
    test_point &test = contest[test_number];
    float esno = test.esno;
    int n_block = test.n_block;
    if (has_default_esno) esno = default_esno;
    if (has_default_nblock) n_block = default_nblock;
    std::cout << "Test " << test_number << " (" << test.n << "," << test.k << "):  ";

    // Run test and output results
    run_test(test.k, test.n, esno, n_block, test.opt_avg, run_stats);
    int n_sample = run_stats.n_sample();
    auto sum = run_stats.sum();
    std::array<float, 4> mean;
    for (int i = 0; i < 4; ++i) {
        mean[i] = ((float)sum[i]) / n_sample;
    }
    //std::cout << n_sample << std::endl;
    std::cout << "Block: " << sum[0] << "/" << n_sample << " = " << mean[0] << ", "
              << "Info Bit Errors: " << sum[1]  << "/" << n_sample*contest[test_number].k << " = " << mean[1]/contest[test_number].k << ", "
              << "Encoding Time (ns): " << sum[2]  << "/" << n_sample << " = " << mean[2] << ", "
              << "Decoding Time (ns): " << sum[3]  << "/" << n_sample << " = " << mean[3] << ", " << std::endl;
}

void run_test_file(std::string filename, std::string output_filename) {
    decoder_stats run_stats;

    // Open test parameter file using filename
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    // Setup output
    std::ostream* outputStream = &std::cout;
    std::ofstream fileStream;
    if (!output_filename.empty()) {
      fileStream.open(output_filename + ".out");
      if (fileStream.is_open()) {
        outputStream = &fileStream;
      } else {
        std::cerr << "Error opening output file: " << output_filename << ".out" << std::endl;
      }
    }

    // Start line by line file read until no more lines
    std::string line;
    while (std::getline(file, line)) {
        // Skip blank/whitespace-only lines and comment lines
        auto pos = line.find_first_not_of(" \t\r\n");
        if (pos == std::string::npos) continue;   // blank line
        if (line[pos] == '#') continue;           // comment line
        std::istringstream iss(line);
        int k, n, n_block, opt_avg;
        float esno;

        // For each line, read 4 parameters: int k, int n, float esno, int n_block
        if (!(iss >> k >> n >> esno >> n_block >> opt_avg)) {
            std::cerr << "Error reading line: " << line << std::endl;
            continue;
        }

        if (k <= 0 || n <= k || esno < 0.0f || n_block <= 0) {
            std::cerr << "Invalid test parameters in line: " << line << std::endl;
            continue;
        }

        const float effective_esno = has_default_esno ? default_esno : esno;
        const int effective_nblock = has_default_nblock ? default_nblock : n_block;

        // Run test with given parameters and command-line overrides.
        run_test(k, n, effective_esno, effective_nblock, opt_avg, run_stats);

        // Process results
        int n_sample = run_stats.n_sample();
        if (n_sample == 0) continue;
        auto sum = run_stats.sum();
        std::array<float, 4> mean;
        for (int i = 0; i < 4; ++i) {
            mean[i] = ((float)sum[i]) / n_sample;
        }

        // Write results
        *outputStream << k << " " << n << " "  << effective_esno << " " << effective_nblock << " " << sum[0] << " " << sum[1] << " " << mean[2] << " " << mean[3] << std::endl;

        // Print results
        std::cout<< "Test with parameters (k=" << k << ", n=" << n << ", esno=" << effective_esno << ", n_block=" << effective_nblock << "): "
                  << "Block: " << sum[0] << "/" << n_sample << " = " << mean[0] << ", "
                  << "Info Bit Errors: " << sum[1]  << "/" << n_sample*k << " = " << mean[1]/k << ", "
                  << "Encoding Time (ns): " << sum[2]  << "/" << n_sample << " = " << mean[2] << ", "
                  << "Decoding Time (ns): " << sum[3]  << "/" << n_sample << " = " << mean[3] << ", " << std::endl;

        // Write stats
        if (!output_filename.empty()) {
          std::string suffix = "_" + std::to_string(k) + "_" + std::to_string(n) + "_" + std::to_string(n_block);
          std::ofstream statStream(output_filename + suffix);
          run_stats.print(&statStream);
        }
    }
    file.close();
}

// Setup option for argmin parsing
OptionSpec options[] = {
    {"-h", "--help",   false, "Show this help message"},
    {"-t", "--test",  true,  "Choose the test or use 'all'"},
    {"-s", "--esno",  true,  "Use this Es/N0"},
    {"-m", "--blocks",  true,  "Run this number of blocks"},
    {"-f", "--file",  true,  "Run tests as described in file"},
    {"-o", "--output",  true,  "Write output to a file with this filename"},
    {"-e", "--seed",  true,  "Set random number generator seed"},
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

    try {
    // Declare test_file variable
    std::string output_file;
    std::string test_file;

    // Handle output file argument
    auto iter = parsedOptions.find("output");
    if (iter != parsedOptions.end()) {
        output_file = iter->second;
        std::cout << "Output file = " << output_file << std::endl;
    }
    // Handle seed parameter
    iter = parsedOptions.find("seed");
    if (iter != parsedOptions.end()) {
        rng_seed = std::stoul(iter->second);
        std::cout << "RNG seed = " << rng_seed << std::endl;
    }
    // Handle EsN0 and blocks parameters
    iter = parsedOptions.find("esno");
    if (iter != parsedOptions.end()) {
        default_esno = std::stof(iter->second);
        if (default_esno < 0.0f) throw std::invalid_argument("Es/N0 must be non-negative");
        has_default_esno = true;
        std::cout << "EsN0 = " << default_esno << std::endl;
    }
    iter = parsedOptions.find("blocks");
    if (iter != parsedOptions.end()) {
        default_nblock = std::stoi(iter->second);
        if (default_nblock <= 0) throw std::invalid_argument("blocks must be positive");
        has_default_nblock = true;
        std::cout << "n_block = " << default_nblock << std::endl;
    }
    // Apply overrides before processing a parameter file, independent of option order.
    iter = parsedOptions.find("file");
    if (iter != parsedOptions.end()) {
        test_file = iter->second;
        std::cout << "Input file = " << test_file << std::endl;
        run_test_file(test_file,output_file);
    }
    // Handle test argument
    iter = parsedOptions.find("test");
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
    } catch (const std::exception &error) {
        std::cerr << "Invalid option value: " << error.what() << std::endl;
        return 1;
    }
}
