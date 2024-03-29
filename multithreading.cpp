#include <random>
#include <unordered_map>
#include <tracking_allocator.h>
#include "libcuckoo/cuckoohash_map.hh"
#include <unistd.h>

using namespace std;

template<typename ConcurrentHashMap>
static void RunBenchmark(size_t threads_count,
                         ConcurrentHashMap &map,
                         vector<uint64_t> &queries,
                         vector<bool> &locking_indexes
                         ) {
  vector<thread> threads;
  size_t num_items = queries.size();
  threads.reserve(threads_count);
  uint64_t batch_size = num_items / threads_count;
  auto inserts = [&](uint64_t thread_id) {
    size_t from = thread_id * batch_size;
    size_t to = min((thread_id + 1) * batch_size, num_items);
    for (; from < to; from++) {
      if (locking_indexes[from]) {
        auto locked_tale = map.lock_table();
        cerr << "locked table gloablly" << endl;
        usleep(400'000); // sleep 100 ms
        locked_tale.unlock();
        cerr << "unlocked table gloablly" << endl;
      }
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
  return std::chrono::duration_cast<std::chrono::microseconds>(end - start)
      .count();
}

struct Info {
  uint version;
};

bool operator==(const TrackingAllocator<std::pair<const uint64_t, uint64_t>> &lhs,
                const TrackingAllocator<std::pair<const uint64_t, uint64_t>> &rhs) {
  return true;
}

int main() {
  vector<uint64_t> times_tracked;
  vector<uint64_t> times_untracked;

  vector<uint64_t> queries(10'000'000);
  vector<bool> lockings(10'000'000, false);

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

  for (auto i = 0; i < 100; i++) {
    uint64_t rand  = static_cast<size_t>(dist(gen));
    lockings[rand % lockings.size()] = false;
  }

  using Key = uint64_t;
  using T = uint64_t;
  using AllocatorType = TrackingAllocator<std::pair<const Key, T>>;

  for (auto threads : {12}) {
//  for (auto threads = 1; threads < 32; threads++) {
    std::cout << "run benchmark with " << threads << " threads" << std::endl;
    size_t size;

    libcuckoo::cuckoohash_map<
        Key,
        T,
        std::hash<Key>,
        std::equal_to<>,
        AllocatorType,
        libcuckoo::DEFAULT_SLOT_PER_BUCKET>
        tracked_map(size, (1u << 20) * 4);

    libcuckoo::cuckoohash_map<Key, T> untracked_map;

    times_tracked.emplace_back(Timing([&]() { RunBenchmark(threads, tracked_map, queries, lockings); }));
    std::cout << "capacity: " << tracked_map.capacity() << std::endl;
    std::cout << "size: " << (size >> 20) << "MB" << std::endl;

    times_untracked.emplace_back(Timing([&]() { RunBenchmark(threads, untracked_map, queries, lockings); }));

    auto locked_table = untracked_map.lock_table();

    assert(locked_table.size() == expected_result.size());
    // validate results
    for (auto it = locked_table.begin(); it != locked_table.end(); it++)
      assert(it->second == expected_result.find(it->first)->second);
  }

  // print times
  for (auto time : times_tracked)
    cout << time << endl;

  cout << "++++++++++++++++++++++++++++++" << endl;

  for (auto time : times_untracked)
    cout << time << endl;
}
