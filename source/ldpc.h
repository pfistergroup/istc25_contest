#ifndef LDPC_H
#define LDPC_H

#include "enc_dec.h"
#include <string>
#include <vector>
#include <string>


// Class for generating, encoding, and decoding binary low-density parity-check codes
class ldpc
{
  public:
    // Size and rank
    int n_rows, n_cols, n_edges, rank;

    // Sparse binary matrix given by list row nad col positions of non-zero elements
    intvec row, col;
    
    // Constructor
    ldpc() : n_rows(0), n_cols(0), n_edges(0), rank(0) {}

    // Load code from file in alist format
    void load_alist(std::string &filename);

    // Setup code with r rows, c cols, and row/col degrees given by rd and cd
    void random(int r,int c, std::vector<int> &rd, std::vector<int> &cd);

    // Generate encoder
    void create_encoder();

    // Belief-propagation decoding
    int decode(llrvec &llr_in, int n_iter, llrvec &llr_out);
 
    // Encode info bits into n_cols codeword bits
    void encode(bitvec &info, bitvec &cw);
    // Write code to file in alist format
    void write_alist(const std::string &filename);
    // Sort the edge list lexicographically
    void sort_edges();
};

// Create enc_dec that uses ldpc
class ldpc_enc_dec : public enc_dec
{
  private:
    ldpc ldpc_code;

  public:
    // Constructor
    ldpc_enc_dec();

    // Destructor
    ~ldpc_enc_dec();

    // Setup for [n,k] code
    int init(int k, int n);

    // Encode k info bits into n codeword bits
    void encode(bitvec &info, bitvec &cw);

    // Decode n llrs into n codeword bits and k info bits, return -1 if detected error
    int decode(llrvec &llr, bitvec &cw_est, bitvec &info_est);
};


void test_no_error(ldpc &code);
void test_single_error(ldpc &code);
void test_gaussian_noise(ldpc &code, float esno);

void test_alist_read_write();

#endif // LDPC_H

