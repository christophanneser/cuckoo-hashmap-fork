#include <random>
#include <unordered_map>
#include "libcuckoo/cuckoohash_map.hh"

using namespace std;

static void RunBenchmark(size_t threads_count,
                         libcuckoo::cuckoohash_map<uint64_t, uint64_t> &map,
                         vector<uint64_t> &queries) {
  vector<thread> threads;
  size_t num_items = queries.size();
  threads.reserve(threads_count);
  uint64_t batch_size = num_items / threads_count;
  auto inserts = [&](uint64_t thread_id) {
    size_t from = thread_id * batch_size;
    size_t to = min((thread_id + 1) * batch_size, num_items);
    for (; from < to; from++) {
      size_t key = queries[from];
      map.upsert(
          key, [](uint64_t &v) { v++; }, 1);
    }
  };

  for (auto thread_id = 0; thread_id < threads_count; thread_id++)
    threads.emplace_back(thread(inserts, thread_id));

  for (auto &thread : threads)
    thread.join();
}

static uint64_t Timing(std::function<void()> fn) {
  const auto start = std::chrono::high_resolution_clock::now();
  fn();
  const auto end = std::chrono::high_resolution_clock::now();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
      .count();
}

struct Info {
  uint version;
};

int main() {
  vector<uint64_t> times;
  std::cout << sizeof(std::tuple<int>) << std::endl;

  vector<uint64_t> queries(100'000'000);

  std::random_device rd{};
  std::mt19937 gen{rd()};
  std::normal_distribution<double> dist{5'000'000, 100'000};

  std::unordered_map<uint64_t, uint64_t> expected_result;
  for (auto &query : queries) {
    query = static_cast<size_t>(dist(gen));
    auto it = expected_result.find(query);
    if (it == expected_result.end())
      expected_result.emplace(query, 1);
    else
      it->second++;
  }

  for (auto threads : {1, 2, 4, 8, 16, 32}) {
    std::cout << "run benchmark with " << threads << " threads" << std::endl;
    libcuckoo::cuckoohash_map<uint64_t, uint64_t> map;
    times.emplace_back(Timing([&]() { RunBenchmark(threads, map, queries); }));
    auto locked_table = map.lock_table();

    assert(locked_table.size() == expected_result.size());
    // validate resutls
    for (auto it = locked_table.begin(); it != locked_table.end(); it++)
      assert(it->second == expected_result.find(it->first)->second);
  }

  for (auto time : times)
    cout << time << endl;
}
