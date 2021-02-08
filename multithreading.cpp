#include <libcuckoo/cuckoohash_map.hh>

using namespace std;

static void RunBenchmark(size_t threads_count, size_t num_items) {
  libcuckoo::cuckoohash_map<uint64_t, uint64_t> map;
  vector<thread> threads;
  threads.reserve(threads_count);
  uint64_t batch_size = num_items / threads_count;

  auto inserts = [&](uint64_t thread_id) {
    size_t from = thread_id * batch_size;
    size_t to = min((thread_id + 1) * batch_size, num_items);
    for (; from < to; from++)
      map.insert(from, from);
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

int main() {
  vector<uint64_t> times;

  for (auto threads : {1, 2, 4, 8, 16, 32})
    times.emplace_back(Timing([&]() { RunBenchmark(threads, 100'000'000); }));

  for (auto time: times)
    cout << time << endl;
}
