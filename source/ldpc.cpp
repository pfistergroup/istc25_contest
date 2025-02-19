#include <cmath>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <set>
#include <vector>
#include <random>
#include <fstream>
#include "ldpc.h"

// Load code from file in alist format
void ldpc::load_alist(std::string &filename) {
    // Open file
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    // Clear row / col vectors
    row.clear();
    col.clear();

    // Read basic info
    n_edges = 0;
    int max_col_weight, max_row_weight;
    file >> n_cols >> n_rows;
    file >> max_col_weight >> max_row_weight;

    // Init and read row / col weights
    intvec row_weights(n_rows);
    intvec col_weights(n_cols);
    for (int j = 0; j < n_cols; ++j) {
        file >> col_weights[j];
    }
    for (int i = 0; i < n_rows; ++i) {
        file >> row_weights[i];
    }

    // Read indices of non-zero entries
    for (int j = 0; j < n_cols; ++j) {
        for (int i = 0; i < col_weights[j]; ++i) {
            int row_index;
            file >> row_index;
            col.push_back(j);
            row.push_back(row_index - 1); // Convert to zero-based index
            n_edges++;
        }
    }
    file.close();
}

void ldpc::sort_edges() {
    std::vector<std::pair<int, int>> edges;
    for (size_t i = 0; i < row.size(); ++i) {
        edges.emplace_back(row[i], col[i]);
    }

    // Sort edges lexicographically
    std::stable_sort(edges.begin(), edges.end(), [](const std::pair<int, int> &a, const std::pair<int, int> &b) {
        return a.first < b.first || (a.first == b.first && a.second < b.second);
    });

    // Update row and col vectors
    for (size_t i = 0; i < edges.size(); ++i) {
        row[i] = edges[i].first;
        col[i] = edges[i].second;
    }
}

void ldpc::write_alist(const std::string &filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return;
    }

    // Write number of rows and columns
    file << n_rows << " " << n_cols << std::endl;

    // Compute row and column weights
    intvec row_weights(n_rows, 0);
    intvec col_weights(n_cols, 0);
    for (size_t i = 0; i < row.size(); ++i) {
        row_weights[row[i]]++;
        col_weights[col[i]]++;
    }

    // Write max column and max row weight
    file << *std::max_element(col_weights.begin(), col_weights.end()) << " ";
    file << *std::max_element(row_weights.begin(), row_weights.end()) << std::endl;

    // Write row and column weights
    for (int weight : col_weights) {
        file << weight << " ";
    }
    file << std::endl;
    for (int weight : row_weights) {
        file << weight << " ";
    }
    file << std::endl;

    // Write col connections
    for (int j = 0; j < n_cols; ++j) {
        for (size_t i = 0; i < col.size(); ++i) {
            if (col[i] == j) {
                file << row[i] + 1 << " "; // Convert to one-based index
            }
        }
        file << std::endl;
    }
    // Write row connections
    for (int i = 0; i < n_rows; ++i) {
        for (size_t j = 0; j < row.size(); ++j) {
            if (row[j] == i) {
                file << col[j] + 1 << " "; // Convert to one-based index
            }
        }
        file << std::endl;
    }

    file.close();
}

// Setup code with r rows, c cols, and row/col degrees given by rd and cd
void ldpc::random(int r, int c, intvec &rd, intvec &cd) {
    // Setup
    n_rows = r;
    n_cols = c;

    // Clear existing row and col vectors
    row.clear();
    col.clear();

    // Create stubs for rows and columns based on degrees
    intvec row_stubs, col_stubs;
    for (int i = 0; i < r; ++i) {
        row_stubs.insert(row_stubs.end(), rd[i], i);
    }
    for (int j = 0; j < c; ++j) {
        col_stubs.insert(col_stubs.end(), cd[j], j);
    }

    bool is_simple = false;
    std::random_device rd_device;
    std::mt19937 generator(rd_device());

    while (!is_simple) {
        // Shuffle the stubs to create random pairings
        std::shuffle(row_stubs.begin(), row_stubs.end(), generator);
        std::shuffle(col_stubs.begin(), col_stubs.end(), generator);

        // Pair the stubs to form edges
        row.clear();
        col.clear();
        for (size_t i = 0; i < row_stubs.size(); ++i) {
            row.push_back(row_stubs[i]);
            col.push_back(col_stubs[i]);
        }

        // Check if the graph is simple
        is_simple = true;
        std::set<std::pair<int, int>> edge_set;
        for (size_t i = 0; i < row.size(); ++i) {
            if (row[i] == col[i] || !edge_set.insert({row[i], col[i]}).second) {
                is_simple = false;
                break;
            }
        }
    }
}

// Generate encoder
void ldpc::create_encoder() {
    // This is a stub implementation
    // Actual implementation would create an encoder for the LDPC code
}


// Belief-propagation decoding
int ldpc::decode(llrvec &llr_in, int n_iter, llrvec &llr_out) {

    // Initialize messages
    size_t n_edges = row.size(); // Calculate number of edges
    llrvec bit_accum(n_cols);
    llrvec check_accum(n_rows);
    llrvec bit_message(n_edges);
    llrvec check_message(n_edges);
    for (size_t i = 0; i < n_edges; ++i) {
        bit_message[i] = llr_in[col[i]];
    }

    // Iterative decoding
    for (int iter = 0; iter < n_iter; ++iter) {
        //std::cout << "Iter " << iter << std::endl;
        // Clip bit messages
        for (size_t i = 0; i < n_edges; ++i) {
            float temp = bit_message[i];
            bit_message[i] = (temp <= 0 ? -1 : 1) * std::max(0.001f, std::min(15.0f, std::abs(temp)));
            //std::cout << bit_message[i] << " ";
        }
        //std::cout << std::endl;

        // Check node update
        std::fill(check_accum.begin(), check_accum.end(), 1.0f);
        for (size_t i = 0; i < n_edges; ++i) {
            check_accum[row[i]] *= std::tanh(bit_message[i]/2.0);
        }
        for (size_t i = 0; i < n_edges; ++i) {
            check_message[i] = 2.0 * std::atanh(check_accum[row[i]]/std::tanh(bit_message[i]/2.0));
            //std::cout << check_message[i] << " ";
        }
        //std::cout << std::endl;

        // Variable node update
        //for (size_t i = 0; i < n_cols; ++i) {
        //    bit_accum[i] = llr_in[i];
        //}
        bit_accum = llr_in;
        for (size_t i = 0; i < n_edges; ++i) {
            bit_accum[col[i]] += check_message[i];
        }
        //std::cout << bit_accum[0] << " ";
        for (size_t i = 0; i < n_edges; ++i) {
            bit_message[i] = bit_accum[col[i]] - check_message[i];
        }
    }
    //std::cout << std::endl;

    // Output
    llr_out = bit_accum;
    //int count = 0;
    //for (size_t i = 0; i < n_cols; ++i) {
    //    if (llr_out[i] <= 0.0f) {
    //        count++;
    //        std::cout << i << "=" << llr_out[i] << " ";
    //    }
    //}
    //std::cout << "Count " << count << std::endl;

    // Check if codeword
    std::vector<int> checks(n_rows, 0);
    for (size_t i = 0; i < n_edges; ++i) {
      if (llr_out[col[i]] != 0.0f) {
        checks[row[i]] ^= (llr_out[col[i]] < 0.0f ? 1 : 0);
      }
      else {
        return 0;
      }
    }

    //for (const auto &val: checks) std::cout << val << " ";
    //std::cout << std::endl;

    // Return true if and only if codeword
    return std::all_of(checks.begin(), checks.end(), [](int value) { return (value==0); });
}

void ldpc::encode(bitvec &info, bitvec &cw) {
    // This is a stub implementation
    // Actual implementation would encode the information bits into codeword bits
    // For now, send 0 cw.
    for (size_t i = 0; i < n_cols; ++i) {
        cw[i] = 0;
    }
}


int ldpc_enc_dec::init(int k, int n) {
    // Initialize the LDPC code randomly
    intvec col_degrees(n, 3); // Example column degrees
    intvec row_degrees(k, 3*n/k); // Example row degrees
   
    ldpc_code.random(k, n, row_degrees, col_degrees);
    //ldpc_code.create_encoder();
    return 0;
}

void ldpc_enc_dec::encode(bitvec &info, bitvec &cw) {
    // Use the LDPC encoder
    ldpc_code.encode(info, cw);
}

int ldpc_enc_dec::decode(llrvec &llr, bitvec &cw_est, bitvec &info_est) {
    // Use the LDPC decoder
    std::vector<llr_type> llr_vec(llr.begin(), llr.end());
    std::vector<llr_type> cw_est_vec(cw_est.begin(), cw_est.end());
    auto result = ldpc_code.decode(llr_vec, 50, cw_est_vec); // Assuming 50 iterations for decoding
    std::copy(cw_est_vec.begin(), cw_est_vec.end(), cw_est.begin());
    return result;
}

