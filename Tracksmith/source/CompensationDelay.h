#pragma once
#include <algorithm>
#include <vector>

// Configure/reset while audio is detached; process owns the cursor.
class CompensationDelay
{
public:
    void configure(int samples)
    {
        if (samples == length) return;
        length = std::max(0, samples); left.assign(static_cast<size_t>(length), 0.0f); right.assign(static_cast<size_t>(length), 0.0f); cursor = 0;
    }
    void reset() noexcept { std::fill(left.begin(), left.end(), 0.0f); std::fill(right.begin(), right.end(), 0.0f); cursor = 0; }
    int samples() const noexcept { return length; }
    void process(float* l, float* r, int count) noexcept
    {
        if (length == 0) return;
        for (int frame = 0; frame < count; ++frame)
        {
            const auto index = static_cast<size_t>(cursor);
            const float oldL = left[index], oldR = right[index]; left[index] = l[frame]; right[index] = r[frame];
            l[frame] = oldL; r[frame] = oldR;
            if (++cursor == length) cursor = 0;
        }
    }
private:
    std::vector<float> left, right; int length = 0, cursor = 0;
};
