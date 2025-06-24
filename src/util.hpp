
#include <fftw3.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <xtensor.hpp>
#include <xtensor/core/xtensor_forward.hpp>
#include <xtensor/views/xslice.hpp>
namespace dsat {
  constexpr size_t BLOCK_SIZE   = 2048;
  constexpr float DIP_THRESHOLD = 0.1f;

  using fftwf_plan_ptr =
    std::unique_ptr<std::remove_pointer_t<fftwf_plan>, void (*)(fftwf_plan)>;

  template <class T>
  inline auto to_xt_adaptor(T* data, size_t len) {
    return xt::adapt(
      data, len, xt::no_ownership(), std::array<size_t, 1> {len});
  }

  inline fftwf_plan_ptr wrap_fftwf(fftwf_plan plan) {
    return fftwf_plan_ptr(plan, fftwf_destroy_plan);
  }

  template<class E>
  inline void zero_buffer(E& buffer) {
    xt::noalias(buffer) = xt::zeros<typename E::value_type>(buffer.shape());
  }

  template <class E>
  inline size_t find_period(const E& cmndf, size_t max_period, float thresh = dsat::DIP_THRESHOLD) {
    size_t period = 0;
    max_period = std::min(max_period, dsat::BLOCK_SIZE - 1);
    while (period < max_period) {
      ++period;
      if (cmndf[period] > thresh) {
        while ((period < max_period) && (cmndf[period] > cmndf[period + 1]))
          ++period;
        return period;
      }
    }
    return 0;
  }
}  // namespace dsat