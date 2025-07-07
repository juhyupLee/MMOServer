#pragma once
class Timer {
    using Clock = std::chrono::high_resolution_clock;
    std::chrono::time_point<Clock> start;
public:
    Timer() : start(Clock::now()) {}
    void stop(const std::string& label) {
        auto end = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << label << ": " << duration.count() / 1000.0 << " ms\n";
    }
};