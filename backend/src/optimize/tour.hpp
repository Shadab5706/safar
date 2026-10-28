#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace safar::optimize {

// Visiting order for a set of stops: a small travelling-salesman problem.
// cost[i][j] = cost of the best journey from stop i to stop j (it need not equal
// cost[j][i]: traffic, one-way fares and walking differ). Stop 0 is the start and
// never moves. With round_trip the tour returns to stop 0 at the end.
struct TourProblem {
    std::vector<std::vector<double>> cost;
    bool round_trip = false;

    std::size_t size() const { return cost.size(); }
};

using Order = std::vector<std::size_t>;  // a permutation of 0..n-1 starting with 0

double tour_cost(const TourProblem& p, const Order& order);

// Neighbour move used by both searches: reverse order[i..j], 1 <= i < j < n (2-opt).
void two_opt(Order& order, std::size_t i, std::size_t j);

struct CurvePoint {
    std::size_t evaluations = 0;  // tours costed so far (the x axis, same unit for every method)
    double current = 0;           // cost of the tour the search is standing on
    double best = 0;              // best cost found so far
    double temperature = 0;       // simulated annealing only
};

struct LocalSearchResult {
    Order order;
    double cost = 0;
    std::size_t evaluations = 0;
    std::size_t moves = 0;           // moves made (accepted)
    std::size_t restarts = 0;        // hill climbing: climbs started
    std::size_t accepted_worse = 0;  // simulated annealing: uphill moves taken
    std::vector<CurvePoint> curve;   // at most max_curve_points, always ends at the final state
    double runtime_ms = 0;
};

// Steepest-ascent hill climbing over 2-opt neighbours, with random restarts. The
// first climb starts from the given order (the order the user typed), the others
// from random shuffles. Each climb stops at a local optimum: no neighbour is better.
struct HillClimbOptions {
    std::size_t restarts = 8;  // climbs in total, >= 1
    std::uint64_t seed = 42;
    std::size_t max_curve_points = 400;
};

// Simulated annealing over random 2-opt moves with a geometric cooling schedule
// T(k+1) = alpha * T(k). A worse tour is accepted with probability exp(-delta / T),
// which lets the search climb out of the local optima that stop hill climbing.
struct AnnealOptions {
    std::size_t iterations = 4000;
    double t0 = 0;     // starting temperature; 0 = pick so ~80% of uphill moves pass at first
    double alpha = 0;  // cooling factor; 0 = cool from t0 to t0 / 1000 over the run
    std::uint64_t seed = 42;
    std::size_t max_curve_points = 400;
};

LocalSearchResult hill_climb(const TourProblem& p, const Order& initial, const HillClimbOptions& opt = {});
LocalSearchResult simulated_annealing(const TourProblem& p, const Order& initial, const AnnealOptions& opt = {});

// The exact optimum by trying every order: (n-1)! tours, so only for n <= max_stops.
std::optional<LocalSearchResult> exhaustive(const TourProblem& p, std::size_t max_stops = 9);

// 0, 1, ..., n-1
Order identity_order(std::size_t n);

}  // namespace safar::optimize
