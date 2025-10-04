#include <myx_coroutine/generator.hpp>
#include <myx_coroutine/task.hpp>

namespace myx_coroutine {

Generator<int> count_to(int n) {
    for (int i = 0; i < n; ++i) {
        co_yield i;
    }
}
} // namespace myx_coroutine
