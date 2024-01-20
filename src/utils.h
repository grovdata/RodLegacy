#pragma once
#include <set>

template <typename T>
T getRandomElement(const std::set<T>& inputSet) {
    if (inputSet.empty()) {
        // Handle the case when the set is empty
        throw std::out_of_range("The set is empty");
    }

    // Use random_device to seed the random number generator
    std::random_device rd;
    // Use the Mersenne Twister engine for randomness
    std::mt19937 gen(rd());

    // Use uniform_int_distribution to get a random index within the set size
    std::uniform_int_distribution<size_t> dist(0, inputSet.size() - 1);

    // Advance to the random element using an iterator
    auto it = inputSet.begin();
    std::advance(it, dist(gen));

    // Return the random element
    return *it;
}