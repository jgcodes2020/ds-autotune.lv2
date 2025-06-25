
#include <fftw3.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <xtensor.hpp>
#include <xtensor/core/xtensor_forward.hpp>
#include <xtensor/views/xslice.hpp>
namespace dsat {
  constexpr size_t BLOCK_SIZE   = 4096;
  constexpr float DIP_THRESHOLD = 0.1f;

  using fftwf_plan_ptr =
    std::unique_ptr<std::remove_pointer_t<fftwf_plan>, void (*)(fftwf_plan)>;

  // Wraps an array into an xtensor value.
  template <class T>
  inline auto to_xt_adaptor(T* data, size_t len) {
    return xt::adapt(data, len, xt::no_ownership(), std::array {len});
  }

  // Wraps a FFTW (float) plan in a std::unique_ptr.
  inline fftwf_plan_ptr wrap_fftwf(fftwf_plan plan) {
    return fftwf_plan_ptr(plan, fftwf_destroy_plan);
  }

  // Zeroes out an xtensor.
  template <class E>
  inline void zero_buffer(E& buffer) {
    xt::noalias(buffer) = xt::zeros<typename E::value_type>(buffer.shape());
  }

  // Estimates the period given a calculation of the CMNDF.
  template <class E>
  inline size_t est_period(
    const E& cmndf, size_t max_period, float thresh = dsat::DIP_THRESHOLD)
    requires std::is_same_v<typename E::value_type, float>
  {
    size_t period = 0;
    max_period = std::min(max_period, dsat::BLOCK_SIZE);
    // search for a suitable period (< max_period).
    while (period < max_period) {
      ++period;
      // A suitable period should dip below our set threshold.
      if (cmndf[period] < thresh) {
        // Advance to a local minimum.
        while ((period < max_period) && (cmndf[period] > cmndf[period + 1]))
          ++period;
        return period;
      }
    }
    return 0;
  }

  // Refines a sample-accurate estimate of period by quadratic interpolation.
  template <class E>
  inline float refine_est_period(const E& cmndf, size_t est) {
    assert(est > 0);

    // Treat the sample cmndf[est] as t = 0. Quadratically interpolate
    // using the two neighbour samples to compute an offset.
    float p = cmndf[est - 1];
    float q = cmndf[est];
    float r = cmndf[est + 1];

    // offset
    float offset = (p - r) * 0.5 / (p + r - 2 * q);

    return (float) est + offset;
  }

}  // namespace dsat