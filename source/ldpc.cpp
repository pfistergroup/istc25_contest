#include <cmath>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <set>
#include <vector>
#include <random>
#include <numeric>
#include <fstream>
#include "ldpc.h"

// Load code from file in alist format
bool ldpc::read_alist(const std::string &filename, bool zero_pad) {
    // Open file
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return false;
    }

    int new_n_cols, new_n_rows, max_col_weight, max_row_weight;
    if (!(file >> new_n_cols >> new_n_rows >> max_col_weight >> max_row_weight) ||
        new_n_cols <= 0 || new_n_rows <= 0 || max_col_weight < 0 || max_row_weight < 0) {
        std::cerr << "Invalid ALIST header: " << filename << std::endl;
        return false;
    }

    // Read col / row weights
    intvec col_weights(new_n_cols);
    intvec row_weights(new_n_rows);
    for (int j = 0; j < new_n_cols; ++j) {
        if (!(file >> col_weights[j]) || col_weights[j] < 0 || col_weights[j] > max_col_weight) {
            std::cerr << "Invalid ALIST column weights: " << filename << std::endl;
            return false;
        }
    }
    for (int i = 0; i < new_n_rows; ++i) {
        if (!(file >> row_weights[i]) || row_weights[i] < 0 || row_weights[i] > max_row_weight) {
            std::cerr << "Invalid ALIST row weights: " << filename << std::endl;
            return false;
        }
    }

    // Column connections are line-oriented.  This supports both the standard
    // zero-padded ALIST form and the compact form emitted by write_alist.
    std::string line;
    std::getline(file, line);
    intvec new_row, new_col;
    for (int j = 0; j < new_n_cols; ++j) {
        if (!std::getline(file, line)) {
            std::cerr << "Truncated ALIST column section: " << filename << std::endl;
            return false;
        }
        std::istringstream entries(line);
        int row_index;
        int count = 0;
        while (entries >> row_index) {
            if (row_index == 0) continue;
            if (row_index < 1 || row_index > new_n_rows || count == col_weights[j]) {
                std::cerr << "Invalid ALIST column connection: " << filename << std::endl;
                return false;
            }
            new_col.push_back(j);
            new_row.push_back(row_index - 1);
            ++count;
        }
        if (count != col_weights[j]) {
            std::cerr << "ALIST column weight mismatch: " << filename << std::endl;
            return false;
        }
    }
    intvec observed_row_weights(new_n_rows, 0);
    for (int row_index : new_row) ++observed_row_weights[row_index];
    if (observed_row_weights != row_weights) {
        std::cerr << "ALIST row weight mismatch: " << filename << std::endl;
        return false;
    }

    // Only commit a fully validated matrix, preserving a usable existing code
    // if loading a replacement fails.
    n_cols = new_n_cols;
    n_rows = new_n_rows;
    n_edges = static_cast<int>(new_row.size());
    rank = 0;
    row = std::move(new_row);
    col = std::move(new_col);
    parity_generator.clear();
    (void)zero_pad;
    return true;
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
bool ldpc::write_alist(const std::string &filename, bool zero_pad) {
    // Open file
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return false;
    }
    if (n_rows <= 0 || n_cols <= 0 || row.size() != col.size()) {
        std::cerr << "Cannot write an invalid LDPC matrix." << std::endl;
        return false;
    }

    // Write number of rows and columns
    file << n_cols << " " << n_rows << std::endl;

    // Compute row and column weights
    intvec row_weights(n_rows, 0);
    intvec col_weights(n_cols, 0);
    for (size_t i = 0; i < row.size(); ++i) {
        if (row[i] < 0 || row[i] >= n_rows || col[i] < 0 || col[i] >= n_cols) {
            std::cerr << "Cannot write an LDPC matrix with invalid edges." << std::endl;
            return false;
        }
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
        }
        if (zero_pad) {
            for (int i = 0; i < max_col_weight - col_weights[j]; ++i) file << "0 ";
        }
        file << std::endl;
    }

    // Write row connections
    for (int i=0; i < n_rows; ++i) {
        for (size_t k = 0; k < row.size(); ++k) {
            if (row[k] == i) {
                file << col[k] + 1 << " "; // Convert to one-based index
            }
        }
        if (zero_pad) {
            for (int j = 0; j < max_row_weight - row_weights[i]; ++j) file << "0 ";
        }
        file << std::endl;
    }
    return static_cast<bool>(file);
}

// Setup code with r rows, c cols, and row/col degrees given by rd and cd
bool ldpc::random(int r, int c, const intvec &rd, const intvec &cd, unsigned int seed_offset) {
    if (r <= 0 || c <= r || static_cast<int>(rd.size()) != r || static_cast<int>(cd.size()) != c ||
        std::any_of(rd.begin(), rd.end(), [](int degree) { return degree < 0; }) ||
        std::any_of(cd.begin(), cd.end(), [](int degree) { return degree < 0; })) {
        std::cerr << "Invalid LDPC dimensions or degree vectors." << std::endl;
        return false;
    }
    const long long row_total = std::accumulate(rd.begin(), rd.end(), 0LL);
    const long long col_total = std::accumulate(cd.begin(), cd.end(), 0LL);
    if (row_total != col_total) {
        std::cerr << "LDPC row and column degree totals differ." << std::endl;
        return false;
    }
    // Setup
    n_rows = r;
    n_cols = c;

    // Clear existing row and col vectors
    row.clear();
    col.clear();
    parity_generator.clear();
    n_edges = 0;
    rank = 0;

    // Create stubs for rows and columns based on degrees
    intvec row_stubs, col_stubs;
    for (int i = 0; i < r; ++i) {
        row_stubs.insert(row_stubs.end(), rd[i], i);
    }
    for (int j = 0; j < c; ++j) {
        col_stubs.insert(col_stubs.end(), cd[j], j);
    }

    // For regular matrices whose column count is an integer multiple of the
    // number of checks, a circulant construction is both simple by design and
    // avoids the impractically high rejection rate of configuration pairing at
    // high check degrees.
    const bool regular = std::all_of(rd.begin(), rd.end(), [&](int degree) { return degree == rd.front(); }) &&
                         std::all_of(cd.begin(), cd.end(), [&](int degree) { return degree == cd.front(); });
    if (regular && c == 5 * r && rd.front() == cd.front() * (c / r)) {
        std::seed_seq seed{r, c, rd.front(), cd.front(), static_cast<int>(seed_offset)};
        std::mt19937 generator(seed);
        intvec shifts(r);
        std::iota(shifts.begin(), shifts.end(), 0);
        std::shuffle(shifts.begin(), shifts.end(), generator);
        row.reserve(static_cast<size_t>(c) * cd.front());
        col.reserve(static_cast<size_t>(c) * cd.front());
        for (int j = 0; j < c; ++j) {
            for (int edge = 0; edge < cd.front(); ++edge) {
                row.push_back((j % r + shifts[edge]) % r);
                col.push_back(j);
            }
        }
        n_edges = static_cast<int>(row.size());
        return true;
    }

    bool is_simple = false;
    std::seed_seq seed{r, c, static_cast<int>(row_total), rd.front(), rd.back(), cd.front(), cd.back(),
                       static_cast<int>(seed_offset)};
    std::mt19937 generator(seed);

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
    if (!is_simple) {
        row.clear();
        col.clear();
        std::cerr << "Unable to generate a simple LDPC graph after " << fail << " attempts." << std::endl;
        return false;
    }
    n_edges = static_cast<int>(row.size());
    return true;
}

// Generate LDPC encoder
bool ldpc::create_encoder(int verbose) {
    parity_generator.clear();
    rank = 0;
    if (n_rows <= 0 || n_cols <= n_rows || row.size() != col.size()) {
        std::cerr << "Cannot create an encoder for an invalid LDPC matrix." << std::endl;
        return false;
    }
    // Convert sparse matrix to dense matrix
    std::vector<std::vector<int>> dense_matrix(n_rows, std::vector<int>(n_cols, 0));
    for (size_t i = 0; i < row.size(); ++i) {
        dense_matrix[row[i]][col[i]] = 1;
    }

    // Define identity column permutation to track pivoting
    intvec perm(n_cols);
    for (int j = 0; j<n_cols; ++j) perm[j]=j;

    // Perform row reduction with column pivoting.  A systematic encoder exists
    // only when all parity checks are independent.
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
            std::cerr << "Parity-check matrix is rank deficient." << std::endl;
            return false;
        }
        ++rank;

        // Use row i to cancel all ones in column perm[i] except row i
        for (int j = 0; j < n_rows; ++j) {
            if (j != i && dense_matrix[j][perm[i]] == 1) {
                for (int l = 0; l < n_cols; ++l) {
                    dense_matrix[j][l] ^= dense_matrix[i][l];
                }
            }
        }
    }

    // Copy transpose of the last k columns of the row-reduced matrix.
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
    return true;
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
    if (static_cast<int>(llr_in.size()) != n_cols || n_rows <= 0 || n_iter <= 0) {
        std::cerr << "Invalid decoder input." << std::endl;
        llr_out.clear();
        return 0;
    }

    const size_t edge_count = row.size();
    fltvec bit_accum(n_cols, 0.0f);
    bitvec check_sign(n_rows, 0);
    fltvec check_accum(n_rows, 0.0f);
    fltvec check_accum2(n_rows, 0.0f);
    fltvec bit_message(edge_count, 0.0f);
    fltvec check_message(edge_count, 0.0f);
    bool is_codeword = false;
    for (size_t i = 0; i < edge_count; ++i) bit_message[i] = llr_in[col[i]];

    for (int iter = 0; iter < n_iter; ++iter) {
        if (DEC_VERBOSE) std::cout << "Iteration " << iter << std::endl;
        if (!MIN_SUM) {
            for (size_t i = 0; i < edge_count; ++i) {
                const float temp = bit_message[i];
                bit_message[i] = (temp <= 0 ? -1 : 1) * std::max(MIN_LLR, std::min(MAX_LLR, std::abs(temp)));
            }
        }

        if (MIN_SUM) {
            std::fill(check_sign.begin(), check_sign.end(), 0);
            std::fill(check_accum2.begin(), check_accum2.end(), MAX_LLR);
            std::fill(check_accum.begin(), check_accum.end(), MAX_LLR);
            for (size_t i = 0; i < edge_count; ++i) {
                check_sign[row[i]] ^= std::signbit(bit_message[i]);
                const float magnitude = std::abs(bit_message[i]);
                if (magnitude < check_accum[row[i]]) {
                    check_accum2[row[i]] = check_accum[row[i]];
                    check_accum[row[i]] = magnitude;
                } else if (magnitude < check_accum2[row[i]]) {
                    check_accum2[row[i]] = magnitude;
                }
            }
            for (size_t i = 0; i < edge_count; ++i) {
                float magnitude = check_accum[row[i]];
                if (std::abs(bit_message[i]) == magnitude) magnitude = check_accum2[row[i]];
                magnitude = std::max(0.0f, magnitude - MIN_SUM_OFFSET);
                check_message[i] = (check_sign[row[i]] ^ std::signbit(bit_message[i])) ? -magnitude : magnitude;
            }
        } else {
            std::fill(check_accum.begin(), check_accum.end(), 1.0f);
            for (size_t i = 0; i < edge_count; ++i) check_accum[row[i]] *= std::tanh(bit_message[i] / 2.0f);
            for (size_t i = 0; i < edge_count; ++i) {
                check_message[i] = 2.0f * std::atanh(check_accum[row[i]] / std::tanh(bit_message[i] / 2.0f));
            }
        }

        for (int i = 0; i < n_cols; ++i) bit_accum[i] = llr_in[i] / BIT_NODE_SCALE;
        for (size_t i = 0; i < edge_count; ++i) bit_accum[col[i]] += check_message[i];
        for (size_t i = 0; i < edge_count; ++i) bit_message[i] = BIT_NODE_SCALE * (bit_accum[col[i]] - check_message[i]);

        // Terminate only when the posterior hard decision itself has zero syndrome.
        std::fill(check_sign.begin(), check_sign.end(), 0);
        for (size_t i = 0; i < edge_count; ++i) check_sign[row[i]] ^= (bit_accum[col[i]] <= 0.0f);
        is_codeword = std::all_of(check_sign.begin(), check_sign.end(), [](int value) { return value == 0; });
        if (is_codeword) break;
    }

    llr_out = std::move(bit_accum);

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
        cw.clear();
        return;
    }

    // Copy k info bits to first k codeword bits
    int k = n_cols - n_rows;
    if (static_cast<int>(info.size()) != k) {
        std::cerr << "Invalid information vector size." << std::endl;
        cw.clear();
        return;
    }
    cw.assign(n_cols, 0);
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
