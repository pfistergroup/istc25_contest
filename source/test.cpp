#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include "ldpc.h"

// Function declarations for tests
void test_no_error(ldpc &code);
void test_single_error(ldpc &code);
int test_gaussian_noise(ldpc &code, float esno, int verbose = 0);
void test_alist_read_write(ldpc &code1);


void test_no_error(ldpc &code) {
    bitvec info(code.n_cols, 0); // Initialize info bits to zero
    bitvec cw(code.n_cols);
    llrvec llr(code.n_cols);
    bitvec cw_est(code.n_cols);
    llrvec llr_est(code.n_cols);

    std::cout << "Running Test No Error..." << std::endl;
    std::cout << "Initial info bits: ";
    for (const auto &bit : info) std::cout << bit << " ";
    std::cout << std::endl;
    code.encode(info, cw);

    std::cout << "Encoded codeword: ";
    for (const auto &bit : cw) std::cout << bit << " ";
    std::cout << std::endl;
    for (size_t i = 0; i < cw.size(); ++i) {
        llr[i] = (cw[i] == 0 ? 1.0f : -1.0f);
    }

    // Decode without errors
    int result = code.decode(llr, 50, llr_est);
    std::cout << "Decoded LLRs: ";
    for (const auto &llr_value : llr_est) std::cout << llr_value << " ";
    std::cout << std::endl;

    if (result == 1) {
        std::cout << "Test No Error: Passed" << std::endl;
    } else {
        std::cout << "Test No Error: Failed" << std::endl;
    }
}

void test_single_error(ldpc &code) {
    bitvec info(code.n_cols, 0); // Initialize info bits to zero
    bitvec cw(code.n_cols);
    llrvec llr(code.n_cols);
    bitvec cw_est(code.n_cols);

    std::cout << "Running Test Single Error..." << std::endl;
    std::cout << "Initial info bits: ";
    for (const auto &bit : info) std::cout << bit << " ";
    std::cout << std::endl;
    code.encode(info, cw);

    std::cout << "Encoded codeword: ";
    for (const auto &bit : cw) std::cout << bit << " ";
    std::cout << std::endl;
    cw[0] = 1 - cw[0];

    std::cout << "Codeword with single error: ";
    for (const auto &bit : cw) std::cout << bit << " ";
    std::cout << std::endl;
    int result = code.decode(llr, 50, llr);
    if (result == 0) {
        std::cout << "Test Single Error: Passed" << std::endl;
    } else {
        std::cout << "Test Single Error: Failed" << std::endl;
    }
}

int test_gaussian_noise(ldpc &code, float esno, int verbose) {
    bitvec info(code.n_cols, 0); // Initialize info bits to zero
    bitvec cw(code.n_cols);
    llrvec llr(code.n_cols);
    bitvec cw_est(code.n_cols);

    code.encode(info, cw);
    std::default_random_engine generator;
    std::normal_distribution<float> distribution(0.0, std::sqrt(1.0 / (2.0 * esno)));
    for (size_t i = 0; i < cw.size(); ++i) {
        llr[i] = (cw[i] == 0 ? 1.0f : -1.0f) + distribution(generator);
    }
    int result = code.decode(llr, 50, llr);

    if (verbose) {
        std::cout << "Running Test Gaussian Noise..." << std::endl;
        std::cout << "Initial info bits: ";
        for (const auto &bit : info) std::cout << bit << " ";
        std::cout << std::endl;

        std::cout << "Encoded codeword: ";
        for (const auto &bit : cw) std::cout << bit << " ";
        std::cout << std::endl;

        std::cout << "LLRs with Gaussian noise: ";
        for (const auto &llr_value : llr) std::cout << llr_value << " ";
        std::cout << std::endl;

        if (result == 0) {
            std::cout << "Test Gaussian Noise: Passed" << std::endl;
        } else {
            std::cout << "Test Gaussian Noise: Failed" << std::endl;
        }
    }

    return result;
}

void test_alist_read_write(ldpc &code1) {
    std::cout << "Running Test Alist Read/Write..." << std::endl;

    // Write the code to disk in alist format
    std::string filename = "test_code.alist";
    code1.write_alist(filename);

    // Instantiate another ldpc class and read the alist back in
    ldpc code2;
    code2.load_alist(filename);

    // Sort edges before comparing
    code1.sort_edges();
    code2.sort_edges();

    std::cout << "Code 1 row indices: ";
    for (const auto &r : code1.row) std::cout << r << " ";
    std::cout << std::endl;

    std::cout << "Code 1 column indices: ";
    for (const auto &c : code1.col) std::cout << c << " ";
    std::cout << std::endl;

    std::cout << "Code 2 row indices: ";
    for (const auto &r : code2.row) std::cout << r << " ";
    std::cout << std::endl;

    std::cout << "Code 2 column indices: ";
    for (const auto &c : code2.col) std::cout << c << " ";
    std::cout << std::endl;
    bool is_equal = (code1.n_rows == code2.n_rows) && (code1.n_cols == code2.n_cols) &&
                    (code1.row.size() == code2.row.size()) && (code1.col.size() == code2.col.size());

    if (is_equal) {
        for (size_t i = 0; i < code1.row.size(); ++i) {
            if (code1.row[i] != code2.row[i] || code1.col[i] != code2.col[i]) {
                is_equal = false;
                break;
            }
        }
    }

    if (is_equal) {
        std::cout << "Test Alist Read/Write: Passed" << std::endl;
    } else {
        std::cout << "Test Alist Read/Write: Failed" << std::endl;
    }
}

int main(int argc, char* argv[])
{
    // Generate short ldpc code
    ldpc code;
    int r = 10; // Example number of rows
    int c = 20; // Example number of columns

    intvec row_degrees(r, 6); // Example row degrees
    intvec col_degrees(c, 3); // Example column degrees
    code.random(r, c, row_degrees, col_degrees);

    // Run test functions
    test_no_error(code); 
    test_single_error(code);
    test_gaussian_noise(code, 5.0, 1); // Example ESNO value
    test_alist_read_write(code);

    // Generate long ldpc code
    std::fill(row_degrees.begin(), row_degrees.end(), 6); // Example row degrees
    std::fill(col_degrees.begin(), col_degrees.end(), 3); // Example column degrees
    std::cout << "gen \n";
    code.random(r, c, row_degrees, col_degrees);
    std::cout << "gen done\n";

    int count = 0;
    for (int i=0; i<10; ++i) {
        if (!test_gaussian_noise(code, 5.0, 0))
            count++;
    }
    std::cout << count << " errors out of 100 trials.\n"; 
}

