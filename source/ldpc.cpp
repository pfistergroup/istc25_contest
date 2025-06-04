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
void ldpc::read_alist(const std::string &filename, bool zero_pad) {
    // Open file
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    // Clear row / col arrays
    row.clear();
    col.clear();

    // Read basic info
    n_edges = 0;
    int max_col_weight, max_row_weight;
    file >> n_cols >> n_rows;
    file >> max_col_weight >> max_row_weight;

    // Read col / row weights
    intvec col_weights(n_cols);
    intvec row_weights(n_rows);
    for (int j = 0; j < n_cols; ++j) {
        file >> col_weights[j];
    }
    for (int i = 0; i < n_rows; ++i) {
        file >> row_weights[i];
    }

    // Read in zero_pad format
    if (zero_pad)
    {
        for (int j = 0; j < n_cols; ++j) {
            for (int i = 0; i < col_weights[j]; ++i) {
                int row_index;
                file >> row_index;
                if (row_index >= 1 && row_index <= n_rows) {
                    col.push_back(j);
                    row.push_back(row_index - 1); // Convert to zero-based index
                    n_edges++;
                }
                else {
                    std::cout << "ldpc::read_alist -- Row index out of range!" << std::endl;
                }
            }
        }
    }
    // Read in variable length format
    else {
        for (int j = 0; j < n_cols; ++j) {
            for (int i = 0; i < max_col_weight; ++i) {
                int row_index;
                file >> row_index;
                if (row_index != 0 && row_index <= n_rows) {
                    col.push_back(j);
                    row.push_back(row_index - 1); // Convert to zero-based index
                    n_edges++;
                }
                else if (row_index != 0) {
                    std::cout << "ldpc::read_alist -- Row index out of range!" << std::endl;
                }
            }
        }
    }
}

// Sort edges to allow comparison between two codes
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

// Write current code to alist
void ldpc::write_alist(const std::string &filename, bool zero_pad) {
    // Open file
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return;
    }

    // Write number of rows and columns
    file << n_cols << " " << n_rows << std::endl;

    // Compute row and column weights
    intvec row_weights(n_rows, 0);
    intvec col_weights(n_cols, 0);
    for (size_t i = 0; i < row.size(); ++i) {
        row_weights[row[i]]++;
        col_weights[col[i]]++;
    }

    // Write max column and max row weight
    int max_col_weight =  *std::max_element(col_weights.begin(), col_weights.end());
    int max_row_weight =  *std::max_element(row_weights.begin(), row_weights.end());
    file << max_col_weight << " ";
    file << max_row_weight << std::endl;

    // Write row and column weights
    for (int weight : col_weights) {
        file << weight << " ";
    }
    file << std::endl;
    for (int weight : row_weights) {
        file << weight << " ";
    }
    file << std::endl;

    // Write column connections
    for (int j = 0; j < n_cols; ++j) {
        for (size_t k = 0; k < col.size(); ++k) {
            if (col[k] == j) {
                file << row[k] + 1 << " "; // Convert to one-based index
            }
            if (zero_pad) {
              for (int i=0; i<max_col_weight-col_weights[j]; ++i) file << "0 ";
            }
        }
        file << std::endl;
    }

    // Write row connections
    for (int i=0; i < n_rows; ++i) {
        for (size_t k = 0; k < row.size(); ++k) {
            if (row[k] == i) {
                file << col[k] + 1 << " "; // Convert to one-based index
            }
            if (zero_pad) {
              for (int j=0; j<max_row_weight-row_weights[i]; ++j) file << "0 ";
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

    int fail = 0;
    while (!is_simple && fail<10000) {
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
            if (is_simple==true && (!edge_set.insert({row[i], col[i]}).second)) {
                is_simple = false;
                fail++;
                break;
            }
        }
    }
    if (fail==10000) std::cout << "Codegen fail " << fail << std::endl;
}

// Generate LDPC encoder
void ldpc::create_encoder(int verbose) {
    // Convert sparse matrix to dense matrix
    std::vector<std::vector<int>> dense_matrix(n_rows, std::vector<int>(n_cols, 0));
    for (size_t i = 0; i < row.size(); ++i) {
        dense_matrix[row[i]][col[i]] = 1;
    }

    // Define identity column permutation to track pivoting
    intvec perm(n_cols);
    for (int j = 0; j<n_cols; ++j) perm[j]=j;

    // Perform row reduction with column pivoting
    for (int i = 0; i < n_rows; ++i) {
        // Search for a non-zero entry in the submatrix
        bool found = false;
        for (int k = i; k < n_cols && !found; ++k) {
            for (int j = i; j < n_rows && !found; ++j) {
                if (dense_matrix[j][perm[k]] == 1) {
                    // Swap columns in permutation
                    std::swap(perm[i], perm[k]);
                    // Swap rows in matrix
                    std::swap(dense_matrix[i], dense_matrix[j]);
                    found = true;
                }
            }
        }
        if (!found) {
            //std::cerr << "Error: Initial no square submatrix is invertible." << std::endl;
            //return;
            break;
        }

        // Use row i to cancel all ones in column perm[i] except row i
        for (int j = 0; j < n_rows; ++j) {
            if (j != i && dense_matrix[j][perm[i]] == 1) {
                for (int l = 0; l < n_cols; ++l) {
                    dense_matrix[j][l] ^= dense_matrix[i][l];
                }
            }
        }
    }

    // Clear existing parity generator matrix and copy transpose of the last k columns of dense matrix
    parity_generator.clear();
    parity_generator.resize(n_cols - n_rows, std::vector<int>(n_rows, 0));
    for (int i = 0; i < n_rows; ++i) {
        for (int j = 0; j < n_cols - n_rows; ++j) {
            parity_generator[j][i] = dense_matrix[i][perm[n_rows + j]];
        }
    }

    // Rearrange to put info block first and parity last
    intvec tmp_perm;
    tmp_perm = perm;
    for (int j = 0; j<n_cols-n_rows; ++j) perm[j]=tmp_perm[n_rows+j];
    for (int j = 0; j<n_rows; ++j) perm[j+n_cols-n_rows]=tmp_perm[j];

    // Print dense_matrix if verbose
    if (verbose) {
        std::cout << "After row reduction with permutation:" << std::endl;
        for (int i = 0; i < n_rows; ++i) {
          for (int j = 0; j < n_cols; ++j) {
            std::cout << dense_matrix[i][perm[j]] << " ";
          }
          std::cout << std::endl;
        }
    }

    // Print dense_matrix if verbose
    //if (verbose) {
    //    std::cout << "After row reduction without permutation:" << std::endl;
    //    for (int i = 0; i < n_rows; ++i) {
    //      for (int j = 0; j < n_cols; ++j) {
    //        std::cout << dense_matrix[i][j] << " ";
    //      }
    //      std::cout << std::endl;
    //    }
    //}

    // Print parity_generator if verbose
    if (verbose) {
        std::cout << "Parity generator:" << std::endl;
        for (int i = 0; i < n_cols - n_rows; ++i) {
          for (int j = 0; j < n_rows; ++j) {
            std::cout << parity_generator[i][j] << " ";
          }
          std::cout << std::endl;
        }
    }

    // Invert permutation so that we can paply to edge list
    intvec invperm(n_cols);
    for (int j = 0; j<n_cols; ++j) invperm[perm[j]] = j;

    // Relabel the bits in the row/col edge list to account for the column pivoting permutation perm
    for (size_t i = 0; i < col.size(); ++i) {
        col[i] = invperm[col[i]];
        if (verbose) std::cout << row[i] << " " << col[i] << std::endl;
    }
}

// Constants
const bool DEC_VERBOSE = 0;
const bool MIN_SUM = 1;
const float BIT_NODE_SCALE = 1.0;
const float MIN_SUM_OFFSET = 0.3;
const float MIN_LLR = 25.0f / 32768.0;
const float MAX_LLR = 17.0f;

// Belief-propagation decoding
int ldpc::decode(fltvec &llr_in, int n_iter, fltvec &llr_out) {

    size_t n_edges = row.size(); // Calculate number of edges
    fltvec bit_accum(n_cols, 0.0f);
    bitvec check_sign(n_rows,0);
    fltvec check_accum(n_rows, 0.0f);
    fltvec check_accum2(n_rows, 0.0f);
    fltvec bit_message(n_edges, 0.0f);
    fltvec check_message(n_edges, 0.0f);
    bool is_codeword;
    for (size_t i = 0; i < n_edges; ++i) {
        bit_message[i] = llr_in[col[i]];
    }

    // Iterative decoding
    for (int iter = 0; iter < n_iter; ++iter) {
        if (DEC_VERBOSE) std::cout << "Iteration " << iter << std::endl;

        // If SUM PRODUCT, clip bit messages
        if (!MIN_SUM) {
            for (size_t i = 0; i < n_edges; ++i) {
                float temp = bit_message[i];
                bit_message[i] = (temp <= 0 ? -1 : 1) * std::max(MIN_LLR, std::min(MAX_LLR, std::abs(temp)));
                if (DEC_VERBOSE) std::cout << bit_message[i] << " ";
            }
            if (DEC_VERBOSE) std::cout << std::endl;
        }

        // Choose MIN SUM versus SUM PRODUCT update
        if (MIN_SUM) {
            // Setup
            std::fill(check_sign.begin(), check_sign.end(), 0);
            std::fill(check_accum2.begin(), check_accum2.end(), MAX_LLR);
            std::fill(check_accum.begin(), check_accum.end(), MAX_LLR);

            // MIN SUM Check node update
            for (size_t i = 0; i < n_edges; ++i) {
                check_sign[row[i]] ^= std::signbit(bit_message[i]);
                if (std::abs(bit_message[i]) < check_accum[row[i]]) {
                    check_accum2[row[i]] = check_accum[row[i]];
                    check_accum[row[i]] = std::abs(bit_message[i]);
                }
                else if (std::abs(bit_message[i]) < check_accum2[row[i]]) {
                    check_accum2[row[i]] = std::abs(bit_message[i]);
                }
            }
            for (size_t i = 0; i < n_edges; ++i) {
                float temp = check_accum[row[i]];
                if (std::abs(bit_message[i])==temp) temp = check_accum2[row[i]];
                temp -= MIN_SUM_OFFSET;
                check_message[i] = (check_sign[row[i]] ^ std::signbit(bit_message[i]) ?  -temp : temp);
                if (DEC_VERBOSE) std::cout << check_message[i] << " ";
            }
            if (DEC_VERBOSE) std::cout << std::endl;
        }
        else {
            // SUM PRODUCT Check node update
            std::fill(check_accum.begin(), check_accum.end(), 1.0f);
            for (size_t i = 0; i < n_edges; ++i) {
                check_accum[row[i]] *= std::tanh(bit_message[i]/2.0);
            }
            for (size_t i = 0; i < n_edges; ++i) {
                check_message[i] = 2.0 * std::atanh(check_accum[row[i]]/std::tanh(bit_message[i]/2.0));
                if (DEC_VERBOSE) std::cout << check_message[i] << " ";
            }
            if (DEC_VERBOSE) std::cout << std::endl;
        }

        // Check for early termination
        if (MIN_SUM) {
            is_codeword = std::all_of(check_sign.begin(), check_sign.end(), [](int value) { return (value==0); });
        }
        else {
            is_codeword = std::all_of(check_accum.begin(), check_accum.end(), [](float value) { return (value>0); });
        }

        // Terminate if all checks satisfied after one iteration
        if (iter>0 && is_codeword) break;
      
        // Variable node update
        for (size_t i = 0; i < n_cols; ++i) {
            bit_accum[i] = llr_in[i]/BIT_NODE_SCALE;
        }
        for (size_t i = 0; i < n_edges; ++i) {
            bit_accum[col[i]] += check_message[i];
        }
        for (size_t i = 0; i < n_edges; ++i) {
            bit_message[i] = BIT_NODE_SCALE*(bit_accum[col[i]] - check_message[i]);
        }
    }

    // Output
    for (size_t j = 0; j < n_cols; ++j) {
        llr_out[j] = bit_accum[j];
    }

    if (DEC_VERBOSE) {
        std::cout << "Decoding finished." << std::endl;
        std::cout << "Output LLRs: ";
        for (const auto &llr_value : llr_out) {
            std::cout << llr_value << " ";
        }
        std::cout << std::endl;
    }

    // Return true if and only if codeword
    if (DEC_VERBOSE) {
        std::cout << "Is codeword: " << is_codeword << std::endl;
        std::cout << "Returning from decode function." << std::endl;
    }
    return is_codeword;
}

// Encode info bitvec into codeword bitvec
void ldpc::encode(bitvec &info, bitvec &cw) {
    // Check if encoder is created
    if (parity_generator.empty()) {
        std::cerr << "Encoder not created. Please create encoder first." << std::endl;
        return;
    }

    // Copy k info bits to first k codeword bits
    int k = n_cols - n_rows;
    for (int i = 0; i < k; ++i) {
        cw[i] = info[i];
    }

    // Compute parity bits
    for (int i = 0; i < n_rows; ++i) {
        int parity = 0;
        for (int j = 0; j < k; ++j) {
            parity ^= (info[j] & parity_generator[j][i]);
        }
        cw[k + i] = parity;
    }
}

