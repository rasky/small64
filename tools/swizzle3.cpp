#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <cstdint>
#include <unordered_map>
#include <algorithm>
#include <random>
#include <chrono>
#include <cmath>
#include <thread>
#include <mutex>
#include <atomic>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <assert.h>

// Upkr library (compiled by top-level Makefile)
#include <upkr.h>

// Include ELFIO (header-only)
#include "elfio/elfio.hpp"

namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
// GLOBAL VARIABLES AND CONSTANTS
// -----------------------------------------------------------------------------

// Fixed load address when building the flat binary.
const uint32_t LOAD_ADDRESS = 0x80000000;

// A constant representing failure.
const size_t MAX_COST = std::numeric_limits<size_t>::max();

// Global constants for gp-relative rule:
const uint32_t GP_RELATIVE_MAX_OFFSET = 65536;    // They must be fully contained within the first 65536 bytes.

// Global flag for graceful termination (CTRL+C).
std::atomic<bool> g_stop{false};
void signalHandler(int signum) {
    g_stop.store(true);
}

// -----------------------------------------------------------------------------
// DATA STRUCTURE: Candidate Section
// -----------------------------------------------------------------------------

// Structure representing a candidate section from an input ELF object file.
struct SectionCandidate {
    std::string file_name;      // The originating file (.o)
    std::string section_name;   // Section name (e.g. ".text", ".data", ".bss", etc.)
    uint32_t alignment;         // Required alignment (power of 2)
    uint32_t size;              // Section size in bytes
    std::vector<uint8_t> data;  // Section contents (for SHT_PROGBITS read from file; for SHT_NOBITS, a zero-filled buffer)
    bool gp_relative;           // True if the section is GP-relative
};

// -----------------------------------------------------------------------------
// UTILITY FUNCTIONS
// -----------------------------------------------------------------------------

// Align an offset upward to a given alignment.
inline uint32_t align_up(uint32_t offset, uint32_t alignment) {
    if (alignment == 0) return offset;
    uint32_t rem = offset % alignment;
    return (rem == 0) ? offset : offset + (alignment - rem);
}

// Build a flat binary buffer from a candidate ordering (permutation).
// The sections are concatenated in order; each section start is aligned properly.
// If any "gp-relative" section (".data" or ".bss" with size < GP_RELATIVE_THRESHOLD)
// would be placed such that it extends beyond GP_RELATIVE_MAX_OFFSET, then an empty buffer is returned.
std::vector<uint8_t> buildFlatBinaryBuffer(const std::vector<SectionCandidate>& candidates,
                                             const std::vector<size_t>& permutation,
                                             const std::vector<uint8_t>& prefix) {
    std::vector<uint8_t> candidateBuffer;
    uint32_t current_offset = 0;
    for (size_t idx : permutation) {
        const SectionCandidate& cand = candidates[idx];
        // Align current offset to the candidate's required alignment.
        current_offset = align_up(current_offset, cand.alignment);
        // Check gp-relative rule: if candidate is gp-relative
        // then it must be entirely within the first GP_RELATIVE_MAX_OFFSET bytes.
        if (cand.gp_relative) {
            if (current_offset + cand.size > GP_RELATIVE_MAX_OFFSET) {
                // Permutation is invalid: return empty buffer.
                return std::vector<uint8_t>();
            }
        }
        // Expand the buffer if needed.
        if (candidateBuffer.size() < current_offset)
        candidateBuffer.resize(current_offset, 0);
        // Append the candidate section data.
        candidateBuffer.insert(candidateBuffer.end(), cand.data.begin(), cand.data.end());
        current_offset += cand.size;
    }
    // Final flat binary: prefix (if any) concatenated with candidateBuffer.
    std::vector<uint8_t> finalBuffer;
    if (!prefix.empty()) {
        finalBuffer = prefix;  // prefix is placed at beginning
        finalBuffer.insert(finalBuffer.end(), candidateBuffer.begin(), candidateBuffer.end());
    } else {
        finalBuffer = candidateBuffer;
    }
    return finalBuffer;
}

// Evaluate a candidate permutation by building the flat binary,
// sending it through upkr, and returning the compressed data size as cost.
// If the flat binary is empty (i.e. invalid due to gp-relative rule), returns MAX_COST.
size_t evaluatePermutation(const std::vector<size_t>& permutation,
                           const std::vector<SectionCandidate>& candidates,
                            const std::vector<uint8_t>& prefix) {
    std::vector<uint8_t> flatBuffer = buildFlatBinaryBuffer(candidates, permutation, prefix);
    if (flatBuffer.empty()) {
        // Permutation is invalid because a gp-relative section lies outside its limit.
        return MAX_COST;
    }
    std::vector<uint8_t> compressedOutput;
    compressedOutput.resize(flatBuffer.size() * 2);

    size_t cost = upkr_compress(compressedOutput.data(), compressedOutput.size(), 
            flatBuffer.data(), flatBuffer.size(), 1);
    return cost;
}

// -----------------------------------------------------------------------------
// SIMULATED ANNEALING FUNCTIONS (MULTIPLE-ROUND APPROACH)
// -----------------------------------------------------------------------------

// Structure to hold one simulated annealing result.
struct SAResult {
    std::vector<size_t> best_permutation;
    size_t best_cost = MAX_COST;
};

// Deterministic simulated annealing run starting from a given candidate ordering.
// 'seed' is fixed for reproducibility. This algorithm works on permutation space.
// Modification: if evaluatePermutation returns MAX_COST, we discard that candidate without temperature processing.
SAResult runSAFromCandidate(
    const std::vector<size_t>& startCandidate,
    const std::vector<SectionCandidate>& candidates,
    const std::vector<uint8_t>& prefix,
    int iterations,
    double initial_temp,
    double cooling_rate,
    unsigned seed
) {
    SAResult result;
    std::vector<size_t> current_perm = startCandidate;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> real_dist(0.0, 1.0);
    std::uniform_int_distribution<size_t> index_dist(0, candidates.size() - 1);

    size_t current_cost = evaluatePermutation(current_perm, candidates, prefix);
    assert(current_cost != MAX_COST);
    if (current_cost == MAX_COST) {
        result.best_permutation = current_perm;
        result.best_cost = MAX_COST;
        return result;
    }
    double temperature = initial_temp;
    result.best_permutation = current_perm;
    result.best_cost = current_cost;

    for (int i = 0; i < iterations && !g_stop.load(); i++) {
        std::vector<size_t> new_perm = current_perm;
        size_t i1 = index_dist(rng);
        size_t i2 = index_dist(rng);
        std::swap(new_perm[i1], new_perm[i2]);

        size_t new_cost = evaluatePermutation(new_perm, candidates, prefix);
        // If the new candidate is invalid, skip the update.
        if(new_cost == MAX_COST) {
            continue;
        }
        int delta = int(new_cost) - int(current_cost);
        if (new_cost < current_cost || real_dist(rng) < std::exp(-delta / temperature)) {
            current_perm = new_perm;
            current_cost = new_cost;
            if (current_cost < result.best_cost ||
               (current_cost == result.best_cost && current_perm < result.best_permutation))
            {
                result.best_cost = current_cost;
                result.best_permutation = current_perm;
            }
        }
        temperature *= cooling_rate;
    }

    return result;
}

void progressBar(int currentStep, int totalSteps, const std::string& status) {
    static int spinnerIndex = 0;
    const char spinnerChars[] = { '|', '/', '-', '\\' };
    const int barLength = 10;
    
    double progressFraction = static_cast<double>(currentStep) / totalSteps;
    int filledLength = static_cast<int>(progressFraction * barLength);
    
    std::string bar = "[";
    for (int i = 0; i < filledLength; ++i) {
        bar += "█";
    }
    for (int i = filledLength; i < barLength; ++i) {
        bar += "░";
    }
    bar += "]";
    
    char spinner = spinnerChars[spinnerIndex % 4];
    spinnerIndex++;
    
    std::cout << "\r" << bar << " " << currentStep << "/" << totalSteps << " " << status << " " << spinner;
    std::cout.flush();
}

// -----------------------------------------------------------------------------
// MAIN: MULTI-ROUND DETERMINISTIC SIMULATED ANNEALING ON SECTION ORDERING
// -----------------------------------------------------------------------------

// This tool takes as command-line arguments an output text file (which will be included
// in a linker script) and one or more input ELF object files (.o).
// It extracts candidate sections (both SHT_PROGBITS and SHT_NOBITS), applies deterministic
// multithreaded simulated annealing (with multiple rounds) to find an optimal ordering of sections
// that minimizes the cost (the compressed size of the flat binary built from the sections).
// Additionally, for data sections (".data" or ".bss") smaller than 1024 bytes (GP-relative),
// they must be placed within the first 65536 bytes; otherwise, the permutation is discarded.
// Finally, the tool outputs a text file listing each candidate section in the optimized order
// (in the format "file_name:section_name") for later use in a linker script.
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " [--prefix prefix_file] <output_include_file> <input1.o> [input2.o] ...\n";
        return EXIT_FAILURE;
    }

    // 1. Parse optional prefix parameter.
    std::vector<uint8_t> prefixBuffer;
    int argIndex = 1;
    bool hasPrefix = false;
    if (std::string(argv[argIndex]) == "--prefix") {
        if (argc < 4) {
            std::cerr << "Error: Missing prefix file after --prefix\n";
            return EXIT_FAILURE;
        }
        hasPrefix = true;
        std::string prefixFile = argv[argIndex + 1];
        std::ifstream ifs(prefixFile, std::ios::binary);
        if (!ifs) {
            std::cerr << "Failed to open prefix file: " << prefixFile << "\n";
            return EXIT_FAILURE;
        }
        prefixBuffer = std::vector<uint8_t>(std::istreambuf_iterator<char>(ifs),
                                            std::istreambuf_iterator<char>());
        ifs.close();
        argIndex += 2;
    }

    if (argc - argIndex < 2) {
        std::cerr << "Usage: " << argv[0] << " [--prefix prefix_file] <output_include_file> <input1.o> [input2.o] ...\n";
        return EXIT_FAILURE;
    }
    std::string outputInclude = argv[argIndex++];
    std::vector<std::string> inputFiles;
    while (argIndex < argc) {
        inputFiles.push_back(argv[argIndex++]);
    }
    std::signal(SIGINT, signalHandler);

    // -------------------------------------------------------------------------
    // 3. Extract Candidate Sections from all Input ELF Object Files.
    // -------------------------------------------------------------------------
    std::vector<SectionCandidate> candidates;
    for (const auto& file : inputFiles) {
        ELFIO::elfio reader;
        if (!reader.load(file)) {
            std::cerr << "Failed to load file " << file << "\n";
            continue;
        }
        for (unsigned i = 0; i < reader.sections.size(); i++) {
            ELFIO::section* sec = reader.sections[i];
            std::string sec_name = sec->get_name();
            // Filter out common metadata.
            if (sec_name == ".symtab" || sec_name == ".strtab" || sec_name == ".shstrtab" || sec_name == ".comment" || sec_name == ".pdr")
                continue;
            if (sec_name.find(".debug") == 0)
                continue;
            if (sec->get_size() == 0)
                continue;
            // Accept sections of type SHT_PROGBITS or SHT_NOBITS.
            if (sec->get_type() != ELFIO::SHT_PROGBITS && sec->get_type() != ELFIO::SHT_NOBITS)
                continue;

            SectionCandidate cand;
            cand.file_name = file;
            cand.section_name = sec_name;
            cand.alignment = sec->get_addr_align();
            if (cand.alignment == 0)
                cand.alignment = 1;
            cand.size = static_cast<uint32_t>(sec->get_size());
            // For SHT_PROGBITS, read raw data; for SHT_NOBITS, generate a zero-filled buffer.
            if (sec->get_type() == ELFIO::SHT_PROGBITS) {
                const char* data = sec->get_data();
                cand.data.assign(data, data + sec->get_size());
            } else if (sec->get_type() == ELFIO::SHT_NOBITS) {
                cand.data.assign(sec->get_size(), 0);
            }
            // If the section name begins with ".sdata" or ".sbss", mark them as gp-relative.
            if (sec_name.find(".sdata") == 0 || sec_name.find(".sbss") == 0) {
                cand.gp_relative = true;
            } else {
                cand.gp_relative = false;
            }
            candidates.push_back(cand);
        }
    }
    if (candidates.empty()) {
        std::cerr << "No candidate sections found in the input files.\n";
        return EXIT_FAILURE;
    }

    // -------------------------------------------------------------------------
    // 4. Initialize Candidate Ordering and Evaluate Its Cost.
    // -------------------------------------------------------------------------
    // Sort candidates by putting first .text sections, then .sdata/.sbss, then .data/.bss.
    std::stable_sort(candidates.begin(), candidates.end(),
        [](const SectionCandidate& a, const SectionCandidate& b) {
            if (a.section_name == ".text" && b.section_name != ".text")
                return true;
            if (a.section_name != ".text" && b.section_name == ".text")
                return false;
            if (a.gp_relative && !b.gp_relative)
                return true;
            if (!a.gp_relative && b.gp_relative)
                return false;
            return a.size < b.size;  // Sort by size for equal types.
        });
    std::vector<size_t> globalCandidate(candidates.size());
    for (size_t i = 0; i < candidates.size(); i++)
        globalCandidate[i] = i;
    size_t globalCost = evaluatePermutation(globalCandidate, candidates, prefixBuffer);
    assert(globalCost != MAX_COST);

    // -------------------------------------------------------------------------
    // 5. Multiple-Round Deterministic Simulated Annealing.
    // -------------------------------------------------------------------------
    int rounds = 10;                 // total rounds
    int iterations_per_round = 300;  // iterations per round per thread
    double initial_temp = 1.0;
    double cooling_rate = 0.995;
    int thread_count = std::thread::hardware_concurrency();
    if (thread_count < 1)
        thread_count = 1;

    for (int round = 0; round < rounds && !g_stop.load(); round++) {
        std::string status = "Optimizing... (" + std::to_string(globalCost) + " bytes)";
        progressBar(round, rounds, status);
        std::vector<SAResult> roundResults(thread_count);
        std::vector<std::thread> threads;
        for (int t = 0; t < thread_count; t++) {
            threads.push_back(std::thread([&, t, round]() {
                // Each thread uses a fixed seed derived from a constant, round, and thread id.
                unsigned seed = 42 + round * 100 + t;
                roundResults[t] = runSAFromCandidate(globalCandidate, candidates, prefixBuffer,
                                                      iterations_per_round,
                                                      initial_temp, cooling_rate,
                                                      seed);
            }));
        }
        for (auto &th : threads)
            th.join();

        // Merge per-thread results deterministically.
        SAResult bestRound = roundResults[0];
        for (int t = 1; t < thread_count; t++) {
            if (roundResults[t].best_cost < bestRound.best_cost ||
               (roundResults[t].best_cost == bestRound.best_cost &&
                roundResults[t].best_permutation < bestRound.best_permutation))
            {
                bestRound = roundResults[t];
            }
        }
        globalCandidate = bestRound.best_permutation;
        globalCost = bestRound.best_cost;
    }

    std::cout << "\r                                                                \r";
    std::cout.flush();

    // -------------------------------------------------------------------------
    // 6. Output the Optimized Section Order as a Text File.
    // -------------------------------------------------------------------------
    // Output format: *(file_name(section_name)) for each candidate section.
    std::ofstream ofs(outputInclude);
    if (!ofs) {
        std::cerr << "Failed to open output include file: " << outputInclude << "\n";
        return EXIT_FAILURE;
    }
    ofs << "/* Optimized Section Order - Include in Linker Script */\n";
    for (size_t idx : globalCandidate) {
        const SectionCandidate& cand = candidates[idx];
        if (cand.section_name == ".text.demo") {
            ofs << "__stage2_entrypoint = .;\n";
            ofs << "KEEP(" << cand.file_name << "(" << cand.section_name << "))" << "\n";
        } else {
            ofs << cand.file_name << "(" << cand.section_name << ")" << "\n";
        }
    }
    ofs.close();

    return EXIT_SUCCESS;
}
