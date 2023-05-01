// provides filter structs, is probably more extensible than manually implementing the different kinds of filers but also probably slower

#include <unordered_set>
#include "parlay/parallel.h"
#include "parlay/primitives.h"

struct Filter {
    virtual bool filter(int index) = 0;
    virtual parlay::sequence<bool> filter(parlay::sequence<int> indices) = 0;
};

// filter that returns true for all indices
struct NoFilter : public Filter {
    bool filter(int index) { return true; }
    parlay::sequence<bool> filter(parlay::sequence<int> indices) {
        return parlay::sequence<bool>(indices.size(), true);
    }
};

// filter that returns true for indices in the given set
struct SetFilter : public Filter {
    std::unordered_set<int> set;
    SetFilter(std::unordered_set<int> set) : set(set) {}
    bool filter(int index) { return set.find(index) != set.end(); }
    parlay::sequence<bool> filter(parlay::sequence<int> indices) {
        return parlay::map(indices, [&] (int i) { return filter(i); });
    }
};

// filter that returns true for indices in the given range
struct RangeFilter : public Filter {
    int start, end;
    RangeFilter(int start, int end) : start(start), end(end) {}
    bool filter(int index) { return index >= start && index < end; }
    parlay::sequence<bool> filter(parlay::sequence<int> indices) {
        return parlay::map(indices, [&] (int i) { return filter(i); });
    }
};

// filter that returns true if either of the given filters return true
// naturally can be used recursively to combine more than two filters
// might lend itself to a par_do if filters are sufficiently expensive and/or numerous
struct OrFilter : public Filter {
    Filter* f1;
    Filter* f2;
    OrFilter(Filter* f1, Filter* f2) : f1(f1), f2(f2) {}
    bool filter(int index) { return f1->filter(index) || f2->filter(index); }
    parlay::sequence<bool> filter(parlay::sequence<int> indices) {
        parlay::sequence<bool> f1_res = f1->filter(indices);
        parlay::sequence<bool> f2_res = f2->filter(indices);
        return parlay::map_index(f1_res, [&] (size_t i, bool b) { return b || f2_res[i]; });
    }
};

// filter that returns true if both of the given filters return true
// naturally can be used recursively to combine more than two filters
// might lend itself to a par_do if filters are sufficiently expensive and/or numerous
struct AndFilter : public Filter {
    Filter* f1;
    Filter* f2;
    AndFilter(Filter* f1, Filter* f2) : f1(f1), f2(f2) {}
    bool filter(int index) { return f1->filter(index) && f2->filter(index); }
    parlay::sequence<bool> filter(parlay::sequence<int> indices) {
        parlay::sequence<bool> f1_res = f1->filter(indices);
        parlay::sequence<bool> f2_res = f2->filter(indices);
        return parlay::map_index(f1_res, [&] (size_t i, bool b) { return b && f2_res[i]; });
    }
};