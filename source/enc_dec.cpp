#include <iostream>
#include <filesystem>
#include <cmath>
#include "enc_dec.h"
#include "ldpc.h"

ldpc code;
int max_iter;

// Setup for [n,k] code
int enc_dec::init(int k, int n, bool opt_avg_latency) {
    // Contestants should replace this code
    //   This code should initialize the encoder-decoder

    std::string fname = "codes/ldpc_"+std::to_string(n)+"_"+std::to_string(k);
    if (std::filesystem::exists(fname+"_g"))
    {
      // read code
      code.read_alist(fname+"_g");
    }
    // create random code
    else 
    {

    // Here we provide example code that interfaces to a simple LDPC setup
    // Setup random [n,k] code
    int dv, dc;
    if (n==4*k) {
      dv = 3;
      dc = 4;
    }
    if (n==2*k) {
      dv = 3;
      dc = 6;
    }
    // 20*(n-k) = 5*4*(n-k) = 5*k = 4*n
    if (4*n==5*k) {
      dv = 4;
      dc = 20;
    }
    // 15*(n-k) = (15/4)*4*(n-k) = (15/4)*k = 3*n
    if (4*n==5*k) {
      dv = 3;
      dc = 15;
    }
    // 15*(n-k) = (15/4)*4*(n-k) = (15/4)*k = 3*n
    if (4*n==5*k) {
      dv = 2;
      dc = 10;
    }
    int c, r;
    c = n;
    r = n-k;
    intvec row_degrees(r, dc); // Example row degrees
    intvec col_degrees(c, dv); // Example column degrees
    code.random(r, c, row_degrees, col_degrees);
    code.write_alist(fname);
    }

    // Setup encoder
    code.create_encoder();

    // Decoding iterations
    max_iter = 20;
    if (opt_avg_latency==1) max_iter = 50;
    return 0;
}

llr_type enc_dec::llr2int(float float_llr) {
    // Contestants should replace this code
    //   This code should convert a single LLR to the integer representation used by decoder
    return std::round((32768/25.0)*float_llr);
}

// Encode k info bits into n codeword bits
void enc_dec::encode(bitvec &info, bitvec &cw) {
    // Contestants should replace this code
    //   Actual implementation would encode the information bits into codeword bits

    // Encode block
    code.encode(info, cw);
}

// Decode n llrs into n codeword bits and k info bits, return -1 if detected error
int enc_dec::decode(llrvec &llr, bitvec &cw_est, bitvec &info_est) {
    // Contestants should replace this code
    //   Actual implementation would decode the llrs into codeword and information bits

    // Decode using ldpc
    fltvec float_llr(code.n_cols);
    fltvec llrout(code.n_cols);
    for (int j=0; j<code.n_cols; ++j) float_llr[j] = (25.0/32768)*llr[j];
    auto result =  code.decode(float_llr, max_iter, llrout);
    for (int j=0; j<code.n_cols; ++j) cw_est[j] = (llrout[j] <= 0 ? 1 : 0);
    for (int j=0; j<code.n_cols-code.n_rows; ++j) info_est[j] = cw_est[j];
    return result;
}

