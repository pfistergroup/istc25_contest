#include <iostream>
#include <filesystem>
#include <cmath>
#include <limits>
#include <algorithm>
#include "enc_dec.h"
#include "ldpc.h"

enc_dec::~enc_dec() {
    delete code_;
}

// Setup for [n,k] code
int enc_dec::init(int k, int n, bool opt_avg_latency) {
    if (k <= 0 || n <= k) {
        std::cerr << "Invalid code dimensions." << std::endl;
        return -1;
    }
    delete code_;
    code_ = new ldpc;

    // If _g code files exists, use that
    std::string fname = "../codes/ldpc_"+std::to_string(n)+"_"+std::to_string(k);
    if (std::filesystem::exists(fname+"_g"))
    {
      // read code
      if (!code_->read_alist(fname+"_g")) return -1;
      if (code_->n_cols != n || code_->n_cols - code_->n_rows != k) {
        std::cerr << "Configured code dimensions do not match the requested code." << std::endl;
        return -1;
      }
    }
    // Otherwise, create random code
    else 
    {

    // Here we provide example code that interfaces to a simple LDPC setup
    // Setup random [n,k] code
    int dv = 0;
    int dc = 0;
    // rate 1/4
    if (n==4*k) {
      dv = 3;
      dc = 4;
    }
    // rate 1/2
    if (n==2*k) {
      dv = 3;
      dc = 6;
    }
    // 20*(n-k) = 5*4*(n-k) = 5*k = 4*n
    // rate 4/5
    if (4*n==5*k) {
      // Degree two creates rank-deficient component codes frequently; use a
      // degree-three construction with matching row degree instead.
      dv = 3;
      dc = 15;
    }
    if (dv == 0 || dc == 0) {
      std::cerr << "Unsupported code rate for [" << n << "," << k << "]." << std::endl;
      return -1;
    }
    int c, r;
    c = n;
    r = n-k;
    intvec row_degrees(r, dc); // Example row degrees
    intvec col_degrees(c, dv); // Example column degrees
    bool initialized = false;
    for (unsigned int attempt = 0; attempt < 100 && !initialized; ++attempt) {
      if (code_->random(r, c, row_degrees, col_degrees, attempt)) {
        initialized = code_->create_encoder();
      }
    }
    if (!initialized) {
      std::cerr << "Unable to construct a full-rank fallback LDPC code." << std::endl;
      return -1;
    }
    }

    // Setup encoder
    if (std::filesystem::exists(fname+"_g") && !code_->create_encoder()) return -1;

    // Decoding iterations
    max_iter_ = opt_avg_latency ? 50 : 20;
    return 0;
}

llr_type enc_dec::llr2int(float float_llr) {
    if (!std::isfinite(float_llr)) return 0;
    constexpr double scale = 32768.0 / 25.0;
    const double scaled = std::round(scale * static_cast<double>(float_llr));
    if (scaled >= std::numeric_limits<llr_type>::max()) return std::numeric_limits<llr_type>::max();
    if (scaled <= std::numeric_limits<llr_type>::min()) return std::numeric_limits<llr_type>::min();
    return static_cast<llr_type>(scaled);
}

// Encode k info bits into n codeword bits
void enc_dec::encode(bitvec &info, bitvec &cw) {
    if (code_ == nullptr) {
        std::cerr << "Encoder-decoder is not initialized." << std::endl;
        cw.clear();
        return;
    }
    code_->encode(info, cw);
}

// Decode n llrs into n codeword bits and k info bits, return -1 if detected error
int enc_dec::decode(llrvec &llr, bitvec &cw_est, bitvec &info_est) {
    if (code_ == nullptr || static_cast<int>(llr.size()) != code_->n_cols) {
        std::cerr << "Decoder is not initialized or LLR length is invalid." << std::endl;
        cw_est.clear();
        info_est.clear();
        return -1;
    }
    fltvec float_llr(code_->n_cols);
    fltvec llrout;
    for (int j = 0; j < code_->n_cols; ++j) float_llr[j] = (25.0f / 32768.0f) * llr[j];
    const int result = code_->decode(float_llr, max_iter_, llrout);
    cw_est.resize(code_->n_cols);
    info_est.resize(code_->n_cols - code_->n_rows);
    for (int j = 0; j < code_->n_cols; ++j) cw_est[j] = (llrout[j] <= 0.0f ? 1 : 0);
    for (int j = 0; j < code_->n_cols - code_->n_rows; ++j) info_est[j] = cw_est[j];
    return result;
}
