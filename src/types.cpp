#include "types.h"

bool operator<(const CarModel &a, const CarModel &b) {
    return a.name < b.name;
}