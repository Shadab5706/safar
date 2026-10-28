#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <random>

#include "optimize/tour.hpp"

using namespace safar::optimize;

namespace {

// Random asymmetric instance: straight-line distance between random points, each
// direction scaled by its own factor (like traffic or one-way fares).
TourProblem random_problem(std::size_t n, std::uint64_t seed, bool round_trip = false) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> coord(0, 100), skew(0.8, 1.3);
    std::vector<std::pair<double, double>> pts(n);
    for (auto& p : pts) p = {coord(rng), coord(rng)};
    TourProblem p;
    p.round_trip = round_trip;
    p.cost.assign(n, std::vector<double>(n, 0));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            if (i != j) p.cost[i][j] = std::hypot(pts[i].first - pts[j].first, pts[i].second - pts[j].second) * skew(rng);
        }
    }
    return p;
}

bool is_tour(const Order& o, std::size_t n) {
    if (o.size() != n || o.front() != 0) return false;
    Order sorted = o;
    std::sort(sorted.begin(), sorted.end());
    return sorted == identity_order(n);
}

void expect_valid_curve(const LocalSearchResult& r) {
    ASSERT_FALSE(r.curve.empty());
    for (std::size_t k = 1; k < r.curve.size(); ++k) {
        EXPECT_LE(r.curve[k].best, r.curve[k - 1].best + 1e-9) << "best-so-far never goes up";
        EXPECT_GE(r.curve[k].evaluations, r.curve[k - 1].evaluations);
        EXPECT_GE(r.curve[k].current, r.curve[k].best - 1e-9);
    }
    EXPECT_NEAR(r.curve.back().best, r.cost, 1e-9);
    EXPECT_EQ(r.curve.back().evaluations, r.evaluations);
}

}  // namespace

TEST(Optimize, TourCostOpenAndRoundTrip) {
    TourProblem p;
    p.cost = {{0, 1, 5}, {2, 0, 3}, {4, 6, 0}};
    EXPECT_DOUBLE_EQ(tour_cost(p, {0, 1, 2}), 1 + 3);
    EXPECT_DOUBLE_EQ(tour_cost(p, {0, 2, 1}), 5 + 6);
    p.round_trip = true;
    EXPECT_DOUBLE_EQ(tour_cost(p, {0, 1, 2}), 1 + 3 + 4);
    EXPECT_DOUBLE_EQ(tour_cost(p, {0, 2, 1}), 5 + 6 + 2);
}

TEST(Optimize, TwoOptReversesASlice) {
    Order o = {0, 1, 2, 3, 4};
    two_opt(o, 1, 3);
    EXPECT_EQ(o, (Order{0, 3, 2, 1, 4}));
    two_opt(o, 2, 4);
    EXPECT_EQ(o, (Order{0, 3, 4, 1, 2}));
}

TEST(Optimize, ExhaustiveFindsTheObviousOrderOnALine) {
    // Stops on a line at x = 0, 7, 2, 9, 4: from 0 the best open path is left to right.
    const double x[] = {0, 7, 2, 9, 4};
    TourProblem p;
    p.cost.assign(5, std::vector<double>(5, 0));
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 5; ++j) p.cost[i][j] = std::abs(x[i] - x[j]);
    const auto best = exhaustive(p);
    ASSERT_TRUE(best);
    EXPECT_EQ(best->order, (Order{0, 2, 4, 1, 3}));
    EXPECT_DOUBLE_EQ(best->cost, 9);
    EXPECT_EQ(best->evaluations, 24u);  // 4!
    EXPECT_FALSE(exhaustive(random_problem(11, 1)));
}

TEST(Optimize, HillClimbReachesALocalOptimumAndNeverGetsWorse) {
    for (std::uint64_t seed = 1; seed <= 10; ++seed) {
        const auto p = random_problem(8, seed);
        const auto start = identity_order(8);
        HillClimbOptions opt;
        opt.restarts = 1;
        const auto r = hill_climb(p, start, opt);
        ASSERT_TRUE(is_tour(r.order, 8));
        EXPECT_LE(r.cost, tour_cost(p, start) + 1e-9);
        EXPECT_NEAR(r.cost, tour_cost(p, r.order), 1e-9);
        EXPECT_EQ(r.restarts, 1u);
        expect_valid_curve(r);
        // Local optimum: no 2-opt neighbour is better.
        for (std::size_t i = 1; i + 1 < 8; ++i) {
            for (std::size_t j = i + 1; j < 8; ++j) {
                Order nb = r.order;
                two_opt(nb, i, j);
                EXPECT_GE(tour_cost(p, nb), r.cost - 1e-9);
            }
        }
    }
}

TEST(Optimize, RestartsAndAnnealingFindTheOptimum) {
    int hc_hits = 0, sa_hits = 0;
    const int trials = 20;
    for (int t = 0; t < trials; ++t) {
        const auto p = random_problem(8, 100 + t, t % 2 == 0);
        const auto best = exhaustive(p);
        ASSERT_TRUE(best);
        HillClimbOptions hc;
        hc.restarts = 20;
        AnnealOptions sa;
        sa.iterations = 6000;
        const auto h = hill_climb(p, identity_order(8), hc);
        const auto s = simulated_annealing(p, identity_order(8), sa);
        ASSERT_TRUE(is_tour(h.order, 8));
        ASSERT_TRUE(is_tour(s.order, 8));
        EXPECT_GE(h.cost, best->cost - 1e-9);
        EXPECT_GE(s.cost, best->cost - 1e-9);
        hc_hits += h.cost <= best->cost + 1e-6;
        sa_hits += s.cost <= best->cost + 1e-6;
    }
    EXPECT_GE(hc_hits, trials * 8 / 10);
    EXPECT_GE(sa_hits, trials * 8 / 10);
}

TEST(Optimize, AnnealingEscapesLocalOptimaThatStopOneClimb) {
    // Same budget question the Trip Planner chart shows: one hill climb stops at the
    // first local optimum; annealing accepts some worse tours and ends up lower.
    double hc_gap = 0, sa_gap = 0;
    for (std::uint64_t seed = 200; seed < 230; ++seed) {
        const auto p = random_problem(9, seed);
        const auto best = exhaustive(p)->cost;
        HillClimbOptions hc;
        hc.restarts = 1;
        const auto h = hill_climb(p, identity_order(9), hc);
        const auto s = simulated_annealing(p, identity_order(9));
        EXPECT_GT(s.accepted_worse, 0u);
        expect_valid_curve(s);
        hc_gap += (h.cost - best) / best;
        sa_gap += (s.cost - best) / best;
    }
    EXPECT_LT(sa_gap, hc_gap);
}

TEST(Optimize, SeededSearchesAreRepeatable) {
    const auto p = random_problem(9, 7);
    const auto a = simulated_annealing(p, identity_order(9));
    const auto b = simulated_annealing(p, identity_order(9));
    EXPECT_EQ(a.order, b.order);
    EXPECT_EQ(a.evaluations, b.evaluations);
    const auto c = hill_climb(p, identity_order(9));
    const auto d = hill_climb(p, identity_order(9));
    EXPECT_EQ(c.order, d.order);
    EXPECT_LE(c.curve.size(), HillClimbOptions{}.max_curve_points);
    EXPECT_LE(a.curve.size(), AnnealOptions{}.max_curve_points);
}

TEST(Optimize, TinyProblemsAreHandled) {
    TourProblem p;
    p.cost = {{0, 3}, {4, 0}};
    EXPECT_EQ(hill_climb(p, {0, 1}).order, (Order{0, 1}));
    EXPECT_EQ(simulated_annealing(p, {0, 1}).order, (Order{0, 1}));
    EXPECT_DOUBLE_EQ(exhaustive(p)->cost, 3);
}
