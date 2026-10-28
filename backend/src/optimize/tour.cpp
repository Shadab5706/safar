#include "optimize/tour.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>

namespace safar::optimize {

namespace {

using Clock = std::chrono::steady_clock;

double elapsed_ms(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// Keeps at most max points, evenly spaced, always including the first and the last.
std::vector<CurvePoint> thin(std::vector<CurvePoint> curve, std::size_t max) {
    if (max < 2 || curve.size() <= max) return curve;
    std::vector<CurvePoint> out;
    out.reserve(max);
    for (std::size_t k = 0; k < max; ++k) {
        out.push_back(curve[k * (curve.size() - 1) / (max - 1)]);
    }
    return out;
}

// Random order with stop 0 kept first.
Order shuffled(std::size_t n, std::mt19937_64& rng) {
    Order o = identity_order(n);
    std::shuffle(o.begin() + 1, o.end(), rng);
    return o;
}

// Two distinct positions 1 <= i < j < n, uniformly over all pairs.
std::pair<std::size_t, std::size_t> random_move(std::size_t n, std::mt19937_64& rng) {
    std::uniform_int_distribution<std::size_t> pos(1, n - 1);
    std::size_t i = pos(rng), j = pos(rng);
    while (j == i) j = pos(rng);
    return {std::min(i, j), std::max(i, j)};
}

}  // namespace

Order identity_order(std::size_t n) {
    Order o(n);
    std::iota(o.begin(), o.end(), std::size_t{0});
    return o;
}

double tour_cost(const TourProblem& p, const Order& order) {
    double total = 0;
    for (std::size_t k = 0; k + 1 < order.size(); ++k) total += p.cost[order[k]][order[k + 1]];
    if (p.round_trip && order.size() > 1) total += p.cost[order.back()][order.front()];
    return total;
}

void two_opt(Order& order, std::size_t i, std::size_t j) {
    std::reverse(order.begin() + static_cast<std::ptrdiff_t>(i), order.begin() + static_cast<std::ptrdiff_t>(j) + 1);
}

LocalSearchResult hill_climb(const TourProblem& p, const Order& initial, const HillClimbOptions& opt) {
    const auto t0 = Clock::now();
    const std::size_t n = p.size();
    std::mt19937_64 rng(opt.seed);
    LocalSearchResult out;
    out.cost = std::numeric_limits<double>::infinity();
    std::vector<CurvePoint> curve;

    for (std::size_t r = 0; r < std::max<std::size_t>(1, opt.restarts); ++r) {
        Order cur = r == 0 ? initial : shuffled(n, rng);
        double c = tour_cost(p, cur);
        ++out.evaluations;
        ++out.restarts;
        if (c < out.cost) {
            out.cost = c;
            out.order = cur;
        }
        curve.push_back({out.evaluations, c, out.cost, 0});

        // Steepest ascent: look at every 2-opt neighbour, move to the best one, and
        // stop when none is better (a local optimum).
        while (true) {
            double best_c = c;
            std::size_t bi = 0, bj = 0;
            for (std::size_t i = 1; i + 1 < n; ++i) {
                for (std::size_t j = i + 1; j < n; ++j) {
                    two_opt(cur, i, j);
                    const double nc = tour_cost(p, cur);
                    ++out.evaluations;
                    two_opt(cur, i, j);  // undo
                    if (nc < best_c - 1e-9) {
                        best_c = nc;
                        bi = i;
                        bj = j;
                    }
                }
            }
            if (bj == 0) {
                curve.push_back({out.evaluations, c, out.cost, 0});  // the scan that proved the local optimum
                break;
            }
            two_opt(cur, bi, bj);
            c = best_c;
            ++out.moves;
            if (c < out.cost) {
                out.cost = c;
                out.order = cur;
            }
            curve.push_back({out.evaluations, c, out.cost, 0});
        }
    }
    out.curve = thin(std::move(curve), opt.max_curve_points);
    out.runtime_ms = elapsed_ms(t0);
    return out;
}

LocalSearchResult simulated_annealing(const TourProblem& p, const Order& initial, const AnnealOptions& opt) {
    const auto t0 = Clock::now();
    const std::size_t n = p.size();
    std::mt19937_64 rng(opt.seed);
    std::uniform_real_distribution<double> unit(0.0, 1.0);

    LocalSearchResult out;
    Order cur = initial;
    double c = tour_cost(p, cur);
    out.order = cur;
    out.cost = c;
    out.evaluations = 1;
    out.restarts = 1;
    if (n < 3) {
        out.curve = {{1, c, c, 0}};
        out.runtime_ms = elapsed_ms(t0);
        return out;
    }

    // Starting temperature: about 80% of uphill moves from the first tour get accepted.
    double temperature = opt.t0;
    if (temperature <= 0) {
        double sum = 0;
        std::size_t uphill = 0;
        for (int k = 0; k < 64; ++k) {
            auto [i, j] = random_move(n, rng);
            Order probe = cur;
            two_opt(probe, i, j);
            const double d = tour_cost(p, probe) - c;
            ++out.evaluations;
            if (d > 0) {
                sum += d;
                ++uphill;
            }
        }
        temperature = uphill ? -(sum / static_cast<double>(uphill)) / std::log(0.8) : 1.0;
    }
    const std::size_t iters = std::max<std::size_t>(1, opt.iterations);
    const double alpha = opt.alpha > 0 ? opt.alpha : std::pow(1e-3, 1.0 / static_cast<double>(iters));

    std::vector<CurvePoint> curve;
    curve.reserve(iters + 1);
    curve.push_back({out.evaluations, c, out.cost, temperature});
    for (std::size_t k = 0; k < iters; ++k) {
        auto [i, j] = random_move(n, rng);
        two_opt(cur, i, j);
        const double nc = tour_cost(p, cur);
        ++out.evaluations;
        const double delta = nc - c;
        if (delta <= 0 || unit(rng) < std::exp(-delta / temperature)) {
            c = nc;
            ++out.moves;
            if (delta > 1e-9) ++out.accepted_worse;
            if (c < out.cost - 1e-9) {
                out.cost = c;
                out.order = cur;
            }
        } else {
            two_opt(cur, i, j);  // rejected: undo
        }
        temperature *= alpha;
        curve.push_back({out.evaluations, c, out.cost, temperature});
    }
    out.curve = thin(std::move(curve), opt.max_curve_points);
    out.runtime_ms = elapsed_ms(t0);
    return out;
}

std::optional<LocalSearchResult> exhaustive(const TourProblem& p, std::size_t max_stops) {
    const std::size_t n = p.size();
    if (n == 0 || n > max_stops) return std::nullopt;
    const auto t0 = Clock::now();
    LocalSearchResult out;
    out.cost = std::numeric_limits<double>::infinity();
    Order o = identity_order(n);
    do {
        const double c = tour_cost(p, o);
        ++out.evaluations;
        if (c < out.cost) {
            out.cost = c;
            out.order = o;
        }
    } while (std::next_permutation(o.begin() + 1, o.end()));
    out.runtime_ms = elapsed_ms(t0);
    return out;
}

}  // namespace safar::optimize
