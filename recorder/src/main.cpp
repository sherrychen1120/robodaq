#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>

static const char* HELP = R"HELP(
recorder — skeleton
Usage:
  recorder --cams cam_front,cam_side --fps 30 --duration 120
)HELP";

int main(int argc, char** argv) {
  std::vector<std::string> args(argv+1, argv+argc);
  if (args.empty() || std::find(args.begin(), args.end(), std::string("--help")) != args.end()) {
    std::cout << HELP << std::endl;
    return 0;
  }
  std::cout << "[recorder] Starting (skeleton). Args:" << std::endl;
  for (auto& a : args) std::cout << "  " << a << std::endl;
  using clock = std::chrono::steady_clock;
  auto period = std::chrono::milliseconds(33);
  auto start = clock::now();
  for (int i=0; i<60; ++i) {
    auto now = clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count();
    std::cout << "[recorder] tick " << i << " t_ns=" << ns << std::endl;
    std::this_thread::sleep_until(start + (i+1)*period);
  }
  std::cout << "[recorder] Done." << std::endl;
  return 0;
}
