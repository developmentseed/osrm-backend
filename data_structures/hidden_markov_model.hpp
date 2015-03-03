/*

Copyright (c) 2015, Project OSRM contributors
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef HIDDEN_MARKOV_MODEL
#define HIDDEN_MARKOV_MODEL

#include <boost/assert.hpp>

#include <cmath>

#include <limits>
#include <vector>

namespace osrm
{
namespace matching
{
// FIXME this value should be a table based on samples/meter (or samples/min)
constexpr static const double log_2_pi = 1.837877066409346; // std::log(2. * M_PI);

constexpr static const double IMPOSSIBLE_LOG_PROB = -std::numeric_limits<double>::infinity();
constexpr static const double MINIMAL_LOG_PROB = std::numeric_limits<double>::lowest();
constexpr static const unsigned INVALID_STATE = std::numeric_limits<unsigned>::max();
} // namespace matching
} // namespace osrm

// closures to precompute log -> only simple floating point operations
struct EmissionLogProbability
{
    double sigma_z;
    double log_sigma_z;

    EmissionLogProbability(const double sigma_z) : sigma_z(sigma_z), log_sigma_z(std::log(sigma_z))
    {
    }

    double operator()(const double distance) const
    {
        return -0.5 * (osrm::matching::log_2_pi + (distance / sigma_z) * (distance / sigma_z)) -
               log_sigma_z;
    }
};

struct TransitionLogProbability
{
    double beta;
    double log_beta;
    TransitionLogProbability(const double beta) : beta(beta), log_beta(std::log(beta)) {}

    double operator()(const double d_t) const { return -log_beta - d_t / beta; }
};

template <class CandidateLists> struct HiddenMarkovModel
{
    std::vector<std::vector<double>> viterbi;
    std::vector<std::vector<std::pair<unsigned, unsigned>>> parents;
    std::vector<std::vector<float>> path_lengths;
    std::vector<std::vector<bool>> pruned;
    std::vector<bool> breakage;

    const CandidateLists &candidates_list;
    const EmissionLogProbability &emission_log_probability;

    HiddenMarkovModel(const CandidateLists &candidates_list,
                      const EmissionLogProbability &emission_log_probability)
        : breakage(candidates_list.size()), candidates_list(candidates_list),
          emission_log_probability(emission_log_probability)
    {
        for (const auto &l : candidates_list)
        {
            viterbi.emplace_back(l.size());
            parents.emplace_back(l.size());
            path_lengths.emplace_back(l.size());
            pruned.emplace_back(l.size());
        }

        clear(0);
    }

    void clear(unsigned initial_timestamp)
    {
        BOOST_ASSERT(viterbi.size() == parents.size() && parents.size() == path_lengths.size() &&
                     path_lengths.size() == pruned.size() && pruned.size() == breakage.size());

        for (unsigned t = initial_timestamp; t < viterbi.size(); t++)
        {
            std::fill(viterbi[t].begin(), viterbi[t].end(), osrm::matching::IMPOSSIBLE_LOG_PROB);
            std::fill(parents[t].begin(), parents[t].end(), std::make_pair(0u, 0u));
            std::fill(path_lengths[t].begin(), path_lengths[t].end(), 0);
            std::fill(pruned[t].begin(), pruned[t].end(), true);
        }
        std::fill(breakage.begin() + initial_timestamp, breakage.end(), true);
    }

    unsigned initialize(unsigned initial_timestamp)
    {
        BOOST_ASSERT(initial_timestamp < candidates_list.size());

        do
        {
            for (auto s = 0u; s < viterbi[initial_timestamp].size(); ++s)
            {
                viterbi[initial_timestamp][s] =
                    emission_log_probability(candidates_list[initial_timestamp][s].second);
                parents[initial_timestamp][s] = std::make_pair(initial_timestamp, s);
                pruned[initial_timestamp][s] =
                    viterbi[initial_timestamp][s] < osrm::matching::MINIMAL_LOG_PROB;

                breakage[initial_timestamp] =
                    breakage[initial_timestamp] && pruned[initial_timestamp][s];
            }

            ++initial_timestamp;
        } while (breakage[initial_timestamp - 1]);

        if (initial_timestamp >= viterbi.size())
        {
            return osrm::matching::INVALID_STATE;
        }

        BOOST_ASSERT(initial_timestamp > 0);
        --initial_timestamp;

        BOOST_ASSERT(breakage[initial_timestamp] == false);

        return initial_timestamp;
    }
};

#endif // HIDDEN_MARKOV_MODEL
