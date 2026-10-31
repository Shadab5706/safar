#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <map>
#include <thread>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "api/server.hpp"
#include "assistant/assistant.hpp"
#include "assistant/rank.hpp"
#include "assistant/reply.hpp"

using namespace safar;
using assistant::Objective;
using assistant::TripState;
using nlohmann::json;

namespace {

// 2026-10-31 is a Saturday.
const std::string kNow = "2026-10-31T09:00";

const assistant::Assistant& bot() {
    static const auto ds = std::make_shared<const data::Dataset>(data::Dataset::load(SAFAR_TEST_DATA_DIR));
    static const auto a = assistant::Assistant::load(SAFAR_TEST_CONFIG_DIR "/assistant.json", ds);
    return a;
}

std::string id(const std::optional<journey::Endpoint>& e) {
    return e ? assistant::endpoint_id(bot().dataset(), *e) : "none";
}

assistant::Analysis ask(const std::string& message, const TripState& previous = {}) {
    return bot().analyse(message, previous, kNow);
}

}  // namespace

// ===== Text =====

TEST(AssistantText, SplitsAmountsUnitsAndClockTimes) {
    auto t = assistant::tokenize("₹300, 6pm or 2hrs at 18:30!");
    ASSERT_EQ(t.size(), 9u);
    EXPECT_EQ(t[0].text, "rs");
    EXPECT_DOUBLE_EQ(*t[1].number, 300);
    EXPECT_EQ(t[3].text, "pm");
    EXPECT_EQ(t[6].text, "hrs");
    EXPECT_TRUE(t[8].clock);
    EXPECT_DOUBLE_EQ(*t[8].number, 18.5);
}

TEST(AssistantText, ReadsNumberWords) {
    auto t = assistant::tokenize("two hundred rupees, 2k, dedh ghanta, do sau");
    EXPECT_DOUBLE_EQ(*t[0].number, 200);
    EXPECT_DOUBLE_EQ(*t[2].number, 2000);
    EXPECT_DOUBLE_EQ(*t[3].number, 1.5);
    EXPECT_DOUBLE_EQ(*t[5].number, 200);
}

TEST(AssistantText, EditDistance) {
    EXPECT_EQ(assistant::edit_distance("kitten", "sitting"), 3u);
    EXPECT_EQ(assistant::edit_distance("", "abc"), 3u);
    EXPECT_DOUBLE_EQ(assistant::similarity("connaught", "connaught"), 1.0);
    EXPECT_GT(assistant::similarity("conaught", "connaught"), 0.85);
}

// ===== Classifier =====

TEST(AssistantClassifier, LearnsFromExamples) {
    assistant::NaiveBayes nb;
    nb.train({{"cheap cheap low cost", "cheap"}, {"fast quick hurry", "fast"}, {"cheapest fare", "cheap"}, {"quickest hurry", "fast"}});
    EXPECT_EQ(nb.predict({"low", "cost"}).label, "cheap");
    EXPECT_EQ(nb.predict({"in", "a", "hurry"}).label, "fast");
    EXPECT_GT(nb.predict({"hurry"}).probability, 0.5);
}

// Leave-one-out over the training sentences: train on all but one, classify that one.
TEST(AssistantClassifier, LeaveOneOutAccuracy) {
    std::ifstream in(SAFAR_TEST_CONFIG_DIR "/assistant.json");
    const json cfg = json::parse(in);
    std::vector<assistant::NaiveBayes::Example> all;
    for (const auto& [label, list] : cfg["training"].items()) {
        for (const auto& s : list) all.push_back({s.get<std::string>(), label});
    }
    std::map<std::string, std::pair<int, int>> per_label;  // label -> correct, total
    int correct = 0;
    for (std::size_t i = 0; i < all.size(); ++i) {
        std::vector<assistant::NaiveBayes::Example> train = all;
        train.erase(train.begin() + static_cast<long>(i));
        assistant::NaiveBayes nb;
        nb.train(train);
        std::vector<std::string> words;
        for (const auto& t : assistant::tokenize(all[i].text)) words.push_back(t.text);
        const bool ok = nb.predict(words).label == all[i].label;
        correct += ok;
        per_label[all[i].label].first += ok;
        ++per_label[all[i].label].second;
    }
    const double accuracy = static_cast<double>(correct) / static_cast<double>(all.size());
    std::printf("leave-one-out accuracy %.1f%% over %zu sentences\n", accuracy * 100, all.size());
    for (const auto& [label, c] : per_label) std::printf("  %-15s %d/%d\n", label.c_str(), c.first, c.second);
    EXPECT_GT(accuracy, 0.75);
}

TEST(AssistantClassifier, UnseenPhrasingsLandOnTheRightObjective) {
    EXPECT_EQ(ask("i am running very late, from rajiv chowk to cyber hub").state.objective, Objective::Fastest);
    EXPECT_EQ(ask("sabse sasta rasta rajiv chowk se cyber hub").state.objective, Objective::Cheapest);
    EXPECT_EQ(ask("rajiv chowk to cyber hub with heavy luggage").state.objective, Objective::Comfort);
    EXPECT_EQ(ask("rajiv chowk to cyber hub without changing trains").state.objective, Objective::FewestChanges);
    EXPECT_EQ(ask("rajiv chowk to cyber hub").state.objective, Objective::Balanced);
}

// ===== Understanding a message =====

TEST(AssistantUnderstand, FromToAndCheapest) {
    auto a = ask("I want to go from Bennett University to Hauz Khas Village in the least cost");
    EXPECT_EQ(id(a.state.from), "place:bennett-university");
    EXPECT_EQ(id(a.state.to), "place:hauz-khas-village");
    EXPECT_EQ(a.state.objective, Objective::Cheapest);
    EXPECT_FALSE(a.state.budget_rs);
}

TEST(AssistantUnderstand, BudgetAndNumbersInsidePlaceNames) {
    auto a = ask("noida sector 18 to dwarka sector 21 under ₹300");
    EXPECT_EQ(id(a.state.from), "station:noida-sector-18");
    EXPECT_EQ(id(a.state.to), "place:dwarka-sector-21");
    ASSERT_TRUE(a.state.budget_rs);
    EXPECT_DOUBLE_EQ(*a.state.budget_rs, 300);
    EXPECT_FALSE(a.state.max_minutes);

    EXPECT_DOUBLE_EQ(*ask("cp to india gate, budget of 150").state.budget_rs, 150);
    EXPECT_DOUBLE_EQ(*ask("cp to india gate 80 rs max").state.budget_rs, 80);
    EXPECT_FALSE(ask("noida sector 62 to dwarka sector 21").state.budget_rs);
}

TEST(AssistantUnderstand, TimeLimits) {
    auto a = ask("rajiv chowk to cyber hub in 2 hours");
    EXPECT_EQ(id(a.state.from), "station:rajiv-chowk");
    EXPECT_EQ(id(a.state.to), "place:cyber-hub");
    EXPECT_DOUBLE_EQ(*a.state.max_minutes, 120);
    EXPECT_DOUBLE_EQ(*ask("cp to india gate within half an hour").state.max_minutes, 30);
    EXPECT_DOUBLE_EQ(*ask("cp to cyber hub in 1 hr 30 min").state.max_minutes, 90);
    EXPECT_DOUBLE_EQ(*ask("cp to cyber hub dedh ghante mein").state.max_minutes, 90);
}

TEST(AssistantUnderstand, NicknamesAndRoleOrder) {
    auto a = ask("fastest way to the airport from cp");
    EXPECT_EQ(id(a.state.from), "place:connaught-place");
    EXPECT_EQ(id(a.state.to), "place:igi-airport-terminal-3");
    EXPECT_EQ(a.state.objective, Objective::Fastest);
    EXPECT_FALSE(a.notes.empty());  // tells the traveller which terminal it picked
}

TEST(AssistantUnderstand, Hinglish) {
    auto a = ask("mujhe bennett se hauz khas jana hai 200 rupaye mein");
    EXPECT_EQ(id(a.state.from), "place:bennett-university");
    EXPECT_EQ(id(a.state.to), "station:hauz-khas");
    EXPECT_DOUBLE_EQ(*a.state.budget_rs, 200);
}

TEST(AssistantUnderstand, ForgivesTypos) {
    auto a = ask("from conaught place to qutub minarr");
    EXPECT_EQ(id(a.state.from), "place:connaught-place");
    EXPECT_EQ(id(a.state.to), "place:qutub-minar");
    EXPECT_EQ(id(ask("bennet univercity to hkv").state.from), "place:bennett-university");
}

TEST(AssistantUnderstand, DepartureAndArrivalTimes) {
    auto a = ask("from india gate to red fort tomorrow at 6 pm");
    ASSERT_TRUE(a.state.depart_at);
    EXPECT_EQ(*a.state.depart_at, "2026-11-01T18:00");
    EXPECT_EQ(*ask("india gate to red fort at 8:30 am on sunday").state.depart_at, "2026-11-01T08:30");
    EXPECT_EQ(*ask("india gate to red fort in the evening").state.depart_at, "2026-10-31T18:00");
    EXPECT_EQ(*ask("leave in 30 minutes from cp to hkv").state.depart_at, "2026-10-31T09:30");
    EXPECT_FALSE(ask("leave in 30 minutes from cp to hkv").state.max_minutes);

    auto b = ask("reach hauz khas by 11 am from rajiv chowk");  // it is 9:00 now
    EXPECT_EQ(id(b.state.to), "station:hauz-khas");
    EXPECT_DOUBLE_EQ(*b.state.max_minutes, 120);
}

TEST(AssistantUnderstand, Modes) {
    auto only = ask("only metro from kashmere gate to hauz khas");
    EXPECT_TRUE(only.state.modes.metro);
    EXPECT_FALSE(only.state.modes.cab);
    EXPECT_FALSE(only.state.modes.autorickshaw);
    EXPECT_EQ(id(only.state.from), "station:kashmere-gate");

    auto no_cab = ask("kashmere gate to hauz khas, I don't want a cab");
    EXPECT_FALSE(no_cab.state.modes.cab);
    EXPECT_TRUE(no_cab.state.modes.metro);

    auto by_metro = ask("bennett to india gate by metro");
    EXPECT_TRUE(by_metro.state.needs.metro);
    EXPECT_TRUE(by_metro.state.modes.cab);

    EXPECT_EQ(ask("cp to hkv, avoid walking").state.objective, Objective::Comfort);
    EXPECT_TRUE(ask("bike taxi from cp to hkv").state.needs.bike);
}

TEST(AssistantUnderstand, FollowUpsRefineTheTrip) {
    auto first = ask("cheapest way from bennett university to hauz khas village");
    auto second = ask("make it under 150", first.state);
    EXPECT_EQ(id(second.state.from), "place:bennett-university");
    EXPECT_EQ(id(second.state.to), "place:hauz-khas-village");
    EXPECT_DOUBLE_EQ(*second.state.budget_rs, 150);
    EXPECT_EQ(second.state.objective, Objective::Cheapest);

    auto faster = ask("fastest please", second.state);
    EXPECT_EQ(faster.state.objective, Objective::Fastest);
    EXPECT_DOUBLE_EQ(*faster.state.budget_rs, 150);

    auto back = ask("way back", faster.state);
    EXPECT_EQ(id(back.state.from), "place:hauz-khas-village");
    EXPECT_EQ(id(back.state.to), "place:bennett-university");

    auto elsewhere = ask("what about india gate", first.state);
    EXPECT_EQ(id(elsewhere.state.from), "place:bennett-university");
    EXPECT_EQ(id(elsewhere.state.to), "place:india-gate");
}

TEST(AssistantUnderstand, AsksForTheMissingPlace) {
    auto a = ask("I want to go to India Gate");
    EXPECT_FALSE(a.state.from);
    EXPECT_EQ(id(a.state.to), "place:india-gate");
    EXPECT_EQ(a.state.awaiting, TripState::Slot::From);

    auto b = ask("bennett university", a.state);
    EXPECT_EQ(id(b.state.from), "place:bennett-university");
    EXPECT_EQ(id(b.state.to), "place:india-gate");
}

TEST(AssistantUnderstand, SmallTalkAndEverydayWords) {
    EXPECT_EQ(ask("hi").intent, assistant::Intent::Greeting);
    EXPECT_EQ(ask("thank you so much").intent, assistant::Intent::Thanks);
    EXPECT_EQ(ask("what can you do").intent, assistant::Intent::Help);
    EXPECT_TRUE(ask("hello and welcome").places.empty());  // "Welcome" is also a station
    EXPECT_FALSE(ask("welcome metro station to shahdara").places.empty());
}

TEST(AssistantUnderstand, ContextRoundTrip) {
    auto a = ask("only metro from kashmere gate to hauz khas tomorrow at 9 am under 60 within 1 hour");
    const auto j = assistant::to_json(a.state, bot().dataset());
    const auto back = assistant::state_from_json(j, bot().dataset());
    EXPECT_EQ(assistant::to_json(back, bot().dataset()), j);
    EXPECT_THROW(assistant::state_from_json(json{{"from", "place:atlantis"}}, bot().dataset()), std::invalid_argument);
    EXPECT_THROW(assistant::state_from_json(json{{"budget_rs", -5}}, bot().dataset()), std::invalid_argument);
}

// ===== Ranking =====

namespace {

std::vector<assistant::RouteFacts> sample_routes() {
    // A: cheap and slow, B: middle, C: fast cab, D: cheap-ish
    return {{120, 50, 2, 10, 140, true, false, false, true},
            {70, 150, 1, 5, 100, true, true, false, false},
            {45, 400, 0, 0, 120, false, true, false, false},
            {95, 90, 1, 8, 115, true, false, true, false}};
}

std::vector<std::size_t> order_for(const TripState& goal) { return assistant::rank(sample_routes(), goal).order; }

}  // namespace

TEST(AssistantRank, BudgetPutsTheFastestAffordableFirstThenCheapestFirst) {
    TripState g;
    g.budget_rs = 160;
    const auto r = assistant::rank(sample_routes(), g);
    EXPECT_EQ(r.order, (std::vector<std::size_t>{1, 0, 3, 2}));
    EXPECT_EQ(r.feasible, 3u);
    EXPECT_FALSE(r.fit[2].fits);
    EXPECT_DOUBLE_EQ(r.fit[2].over_budget_rs, 240);
}

TEST(AssistantRank, TimeLimitPutsTheCheapestQuickEnoughFirstThenQuickestFirst) {
    TripState g;
    g.max_minutes = 100;
    EXPECT_EQ(order_for(g), (std::vector<std::size_t>{3, 2, 1, 0}));
}

TEST(AssistantRank, StatedObjectives) {
    TripState g;
    g.objective = Objective::Cheapest;
    EXPECT_EQ(order_for(g), (std::vector<std::size_t>{0, 3, 1, 2}));
    g.objective = Objective::Fastest;
    EXPECT_EQ(order_for(g), (std::vector<std::size_t>{2, 1, 3, 0}));
    g.objective = Objective::FewestChanges;
    EXPECT_EQ(order_for(g).front(), 2u);
    g.objective = Objective::Balanced;
    EXPECT_EQ(order_for(g), (std::vector<std::size_t>{1, 3, 2, 0}));  // planner score
}

TEST(AssistantRank, FastestWithinBudgetKeepsOverBudgetRoutesLast) {
    TripState g;
    g.objective = Objective::Fastest;
    g.budget_rs = 160;
    EXPECT_EQ(order_for(g), (std::vector<std::size_t>{1, 3, 0, 2}));
}

TEST(AssistantRank, NothingFitsLeadsWithTheClosest) {
    TripState g;
    g.budget_rs = 40;
    const auto r = assistant::rank(sample_routes(), g);
    EXPECT_EQ(r.feasible, 0u);
    EXPECT_EQ(r.order.front(), 0u);  // the cheapest
}

TEST(AssistantRank, RequiredModes) {
    TripState g;
    g.needs.autorickshaw = true;
    EXPECT_EQ(order_for(g).front(), 3u);
}

TEST(AssistantReply, SentencesAndNotes) {
    TripState g;
    g.budget_rs = 160;
    const auto routes = sample_routes();
    const auto r = assistant::rank(routes, g);
    const auto text = assistant::trip_reply(g, "A", "B", routes, {"a", "b", "c", "d"}, r);
    EXPECT_NE(text.find("3 of 4 routes fit your ₹160 budget"), std::string::npos) << text;
    EXPECT_NE(text.find("cheapest to most expensive"), std::string::npos) << text;
    EXPECT_EQ(assistant::fit_notes(g, routes[2], r.fit[2]).front(), "₹240 over budget");
    EXPECT_EQ(assistant::fit_notes(g, routes[0], r.fit[0]).front(), "₹110 under budget");
    EXPECT_EQ(assistant::format_minutes(65), "1 h 5 min");
    EXPECT_EQ(assistant::format_minutes(120), "2 h");
    EXPECT_EQ(assistant::describe_departure("2026-11-01T18:00", kNow), "tomorrow at 6:00 pm");
}

// ===== API =====

namespace {

class AssistantApi : public ::testing::Test {
protected:
    void SetUp() override {
        safar::Config cfg;
        cfg.host = "127.0.0.1";
        cfg.data_dir = SAFAR_TEST_DATA_DIR;
        cfg.transport_file = SAFAR_TEST_CONFIG_DIR "/transport.json";
        static const safar::AppContext ctx = safar::AppContext::load(cfg);
        server_ = std::make_unique<safar::api::Server>(cfg, ctx);
        port_ = server_->raw().bind_to_any_port(cfg.host);
        thread_ = std::thread([this] { server_->raw().listen_after_bind(); });
        server_->raw().wait_until_ready();
    }
    void TearDown() override {
        server_->stop();
        thread_.join();
    }

    std::pair<int, json> post(const json& body) {
        httplib::Client c("127.0.0.1", port_);
        auto res = c.Post("/api/v1/assistant", body.dump(), "application/json");
        if (!res) return {0, json()};
        return {res->status, json::parse(res->body)};
    }

    std::unique_ptr<safar::api::Server> server_;
    std::thread thread_;
    int port_ = 0;
};

}  // namespace

TEST_F(AssistantApi, BudgetRequestComesBackRankedForTheBudget) {
    auto [status, body] = post({{"message", "bennett university to hauz khas village under 250"}, {"now", kNow}});
    ASSERT_EQ(status, 200) << body.dump();
    const auto& d = body["data"];
    EXPECT_EQ(d["kind"], "routes");
    EXPECT_EQ(d["understood"]["budget_rs"], 250);
    const auto& routes = d["journey"]["routes"];
    ASSERT_GE(routes.size(), 2u);
    EXPECT_EQ(routes[0]["rank"], 1);
    // Fitting routes first, and after the first one, cheapest first among them.
    bool seen_unfit = false;
    for (std::size_t i = 0; i < routes.size(); ++i) {
        const bool fits = routes[i]["match"]["fits"];
        if (fits) { EXPECT_FALSE(seen_unfit) << "a fitting route after one over budget"; }
        seen_unfit |= !fits;
        if (fits) { EXPECT_LE(routes[i]["fare_rs"].get<double>(), 250.5); }
    }
    for (std::size_t i = 2; i < routes.size(); ++i) {
        if (routes[i]["match"]["fits"] == routes[i - 1]["match"]["fits"]) {
            EXPECT_LE(routes[i - 1]["fare_rs"].get<double>(), routes[i]["fare_rs"].get<double>());
        }
    }
    EXPECT_FALSE(d["reply"].get<std::string>().empty());
    EXPECT_FALSE(d["suggestions"].empty());
}

TEST_F(AssistantApi, ConversationAsksThenPlans) {
    auto [s1, b1] = post({{"message", "I need to reach India Gate"}, {"now", kNow}});
    ASSERT_EQ(s1, 200);
    EXPECT_EQ(b1["data"]["kind"], "ask_from");
    EXPECT_TRUE(b1["data"]["journey"].is_null());

    auto [s2, b2] = post({{"message", "rajiv chowk"}, {"context", b1["data"]["context"]}, {"now", kNow}});
    ASSERT_EQ(s2, 200);
    EXPECT_EQ(b2["data"]["kind"], "routes");
    EXPECT_EQ(b2["data"]["understood"]["from"]["id"], "rajiv-chowk");
    EXPECT_EQ(b2["data"]["understood"]["to"]["id"], "india-gate");
}

TEST_F(AssistantApi, SmallTalkAndBadInput) {
    auto [s1, b1] = post({{"message", "hello"}});
    ASSERT_EQ(s1, 200);
    EXPECT_EQ(b1["data"]["kind"], "smalltalk");

    auto [s2, b2] = post({{"message", "xyzzy plugh"}});
    ASSERT_EQ(s2, 200);
    EXPECT_EQ(b2["data"]["kind"], "not_understood");

    EXPECT_EQ(post({{"message", "   "}}).first, 400);
    EXPECT_EQ(post({{"text", "cp to hkv"}}).first, 400);
    EXPECT_EQ(post({{"message", std::string(501, 'a')}}).first, 400);
    EXPECT_EQ(post({{"message", "cp to hkv"}, {"context", {{"from", "place:atlantis"}}}}).first, 400);
    EXPECT_EQ(post({{"message", "cp to hkv"}, {"now", "yesterday"}}).first, 400);
}
