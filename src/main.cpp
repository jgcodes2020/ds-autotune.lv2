#include <algorithm>
#include <array>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <print>
#include <utility>

#include <fftw3.h>
#include <lv2/core/lv2.h>
#include <lv2/core/lv2_util.h>

#include <xtensor.hpp>

#include <lvtk/lvtk.hpp>
#include <lvtk/plugin.hpp>

#include "util.hpp"
#include <xtensor/containers/xstorage.hpp>
#include <xtensor/core/xmath.hpp>
#include <xtensor/core/xnoalias.hpp>
#include <xtensor/core/xtensor_forward.hpp>
#include <xtensor/core/xvectorize.hpp>
#include <xtensor/generators/xbuilder.hpp>
#include <xtensor/misc/xcomplex.hpp>
#include <xtensor/views/xbroadcast.hpp>
#include <xtensor/views/xindex_view.hpp>
#include <xtensor/views/xslice.hpp>
#include <xtensor/views/xview.hpp>

constexpr const char* DS_AUTOTUNE_URI =
  "https://github.com/jgcodes2020/ds-autotune.lv2";

class DsAutotune : public lvtk::Plugin<DsAutotune> {
public:
  enum PortID : uint32_t { PORT_INPUT, PORT_OUTPUT, PORT_EST_FREQ };

  DsAutotune(const lvtk::Args& args);

  void connect_port(uint32_t port, void* data);

  void activate();

  void run(uint32_t nframes);

private:
  // Fills m_buffer0 and performs analysis rounds.
  void fill_block(const float* begin, const float* end);

  // Performs analysis, assuming m_buffer0 is full.
  void analyze_block();

  void compute_cmndf();

  // ports
  const float* mp_input;
  float* mp_output;
  float* mp_est_freq;

  // parameters
  double m_sample_rate;

  // Index into buffer0 for input.
  size_t m_ibuffer_idx;

  // input buffers
  xt::xtensor<float, 1> m_ibuffer0;
  xt::xtensor<float, 1> m_ibuffer1;

  // temp real buffers
  xt::xtensor<float, 1> m_tbuffer0;
  xt::xtensor<float, 1> m_tbuffer1;

  // temp complex buffers.
  xt::xtensor<std::complex<float>, 1> m_tcbuffer0;
  xt::xtensor<std::complex<float>, 1> m_tcbuffer1;

  // FFTW plan: m_tcbuffer0 = FFT(m_buffer0)
  dsat::fftwf_plan_ptr m_fft_fwd0;
  // FFTW plan: m_tcbuffer1 = FFT(m_buffer0)
  dsat::fftwf_plan_ptr m_fft_fwd1;
  // FFTW plan: m_buffer0 = IFFT(m_cbuffer0)
  dsat::fftwf_plan_ptr m_fft_rev;
};

DsAutotune::DsAutotune(const lvtk::Args& args) :
  Plugin(args),
  m_sample_rate(args.sample_rate),
  m_ibuffer_idx(0),
  m_ibuffer0({dsat::BLOCK_SIZE * 4}, 0),
  m_ibuffer1({dsat::BLOCK_SIZE * 4}, 0),
  m_tbuffer0({dsat::BLOCK_SIZE * 4}, 0),
  m_tbuffer1({dsat::BLOCK_SIZE * 2}, 0),
  m_tcbuffer0({dsat::BLOCK_SIZE * 2 + 1}, 0),
  m_tcbuffer1({dsat::BLOCK_SIZE * 2 + 1}, 0),
  m_fft_fwd0(
    dsat::wrap_fftwf(fftwf_plan_dft_r2c_1d(
      dsat::BLOCK_SIZE * 4, m_ibuffer0.data(),
      reinterpret_cast<fftwf_complex*>(m_tcbuffer0.data()), FFTW_ESTIMATE))),
  m_fft_fwd1(
    dsat::wrap_fftwf(fftwf_plan_dft_r2c_1d(
      dsat::BLOCK_SIZE * 4, m_ibuffer1.data(),
      reinterpret_cast<fftwf_complex*>(m_tcbuffer1.data()), FFTW_ESTIMATE))),
  m_fft_rev(
    dsat::wrap_fftwf(fftwf_plan_dft_c2r_1d(
      dsat::BLOCK_SIZE * 4,
      reinterpret_cast<fftwf_complex*>(m_tcbuffer0.data()), m_tbuffer0.data(),
      FFTW_ESTIMATE))) {}

void DsAutotune::connect_port(uint32_t port, void* data) {
  switch (port) {
    case DsAutotune::PORT_INPUT:
      mp_input = (const float*) data;
      break;
    case DsAutotune::PORT_OUTPUT:
      mp_output = (float*) data;
      break;
    case DsAutotune::PORT_EST_FREQ:
      mp_est_freq = (float*) data;
      break;
  }
}

void DsAutotune::activate() {
  dsat::zero_buffer(m_ibuffer0);
  dsat::zero_buffer(m_ibuffer1);
  dsat::zero_buffer(m_tbuffer0);
  // dsat::zero_buffer(m_tbuffer1);
  dsat::zero_buffer(m_tcbuffer0);
  dsat::zero_buffer(m_tcbuffer1);

  m_ibuffer_idx = 0;

  *mp_est_freq = 0.0f;
}

void DsAutotune::run(uint32_t n_samples) {
  this->fill_block(mp_input, mp_input + n_samples);
  // for now, this serves as an identity function
  std::copy(mp_input, mp_input + n_samples, mp_output);
}

void DsAutotune::fill_block(const float* begin, const float* end) {
  using namespace xt::placeholders;

  decltype(m_ibuffer0)::value_type x;

  while (begin != end) {
    size_t n_to_block_end = dsat::BLOCK_SIZE - m_ibuffer_idx;
    size_t n_to_input_end = end - begin;
    if (n_to_input_end >= n_to_block_end) {
      // copy and fill remainder of block
      xt::noalias(
        xt::view(m_ibuffer1, xt::range(m_ibuffer_idx, dsat::BLOCK_SIZE))) =
        dsat::to_xt_adaptor(begin, n_to_block_end);

      // push pointer, reset block counter
      begin += n_to_block_end;
      m_ibuffer_idx = 0;

      // swap buffers
      this->analyze_block();
      std::swap(m_ibuffer0, m_ibuffer1);
    }
    else {
      // not enough samples to fill this block, so fill what we can now
      xt::noalias(
        xt::view(
          m_ibuffer1,
          xt::range(m_ibuffer_idx, m_ibuffer_idx + n_to_input_end))) =
        dsat::to_xt_adaptor(begin, n_to_input_end);

      // push counters
      begin += n_to_input_end;
      m_ibuffer_idx += n_to_input_end;
    }
  }
}

void DsAutotune::analyze_block() {
  using namespace xt::placeholders;
  using namespace std::literals;

  // CONSTANTS
  // =======================

  // powers of (-j), the shifting factor
  static const auto shift_factors =
    xt::xtensor_fixed<std::complex<float>, xt::fixed_shape<4>> {
      1.0f, -1.0if, -1.0f, 1.0if};

  // BUFFER VIEWS
  // =======================

  auto ibuffer_full = xt::concatenate(
    xt::xtuple(
      xt::view(m_ibuffer0, xt::range(0, dsat::BLOCK_SIZE)),
      xt::view(m_ibuffer1, xt::range(0, dsat::BLOCK_SIZE))));

  auto tb0_q0 = xt::view(m_tbuffer0, xt::range(0, dsat::BLOCK_SIZE));
  auto tb0_q1 =
    xt::view(m_tbuffer0, xt::range(dsat::BLOCK_SIZE, 2 * dsat::BLOCK_SIZE));

  auto tb1_h0 = xt::view(m_tbuffer1, xt::range(0, dsat::BLOCK_SIZE));
  auto tb1_h1 =
    xt::view(m_tbuffer1, xt::range(dsat::BLOCK_SIZE, 2 * dsat::BLOCK_SIZE));

  // ALGORITHM
  // =======================

  // sum-scan x^2 to tbuffer1
  xt::noalias(m_tbuffer1) = xt::concatenate(
    xt::xtuple(
      xt::xtensor_fixed<float, xt::fixed_shape<1>> {0.0},
      xt::view(xt::cumsum(ibuffer_full * ibuffer_full), xt::range(_, -1))));

  // autocorrelation to tbuffer0
  {
    // FFT both input buffers
    fftwf_execute(m_fft_fwd0.get());
    fftwf_execute(m_fft_fwd1.get());

    // use DFT shifting theorem to move ibuffer1 a quarter cycle back
    auto dft1_shifted = m_tcbuffer1 *
      xt::index_view(shift_factors,
                     xt::arange(size_t(0), dsat::BLOCK_SIZE * 2 + 1) &
                       size_t(3));

    // - merge two halves of buffer in frequency domain
    // - multiply by conj(short) for correlation
    xt::noalias(m_tcbuffer0) =
      (m_tcbuffer0 + dft1_shifted) * xt::conj(m_tcbuffer0);

    // IFFT => tbuffer0
    fftwf_execute(m_fft_rev.get());
  }

  // difference function to tbuffer0
  {
    auto a_sq = xt::broadcast(m_tbuffer1[dsat::BLOCK_SIZE], {dsat::BLOCK_SIZE});
    auto b_sq = tb1_h1 - tb1_h0;

    xt::noalias(tb0_q0) = a_sq + b_sq - tb0_q0 * 2.0f;
  }

  // compute CMNDF to m_tbuffer1
  {
    auto csum        = xt::cumsum(xt::view(tb0_q0, xt::range(1, _)));
    auto arange      = xt::arange(1.0f, (float) dsat::BLOCK_SIZE);
    auto tb1_h0_clip = xt::view(tb1_h0, xt::range(1, _));

    xt::noalias(tb1_h0_clip) = tb1_h0_clip * arange / csum;
    tb1_h0[0]                = 1.0f;
  }

  // estimate period to the nearest sample
  size_t period = 0;
  {
    auto clip      = xt::view(tb1_h0, xt::range(_, -1));
    auto clip_late = xt::view(tb1_h0, xt::range(1, _));
    period = xt::argmax((clip < dsat::DIP_THRESHOLD) && (clip < clip_late))();
  }

  // submit estimated period to output
  // auto period = dsat::est_period(tb1_h0, 1024);
  if (period != 0) {
    *mp_est_freq = (float) m_sample_rate / dsat::refine_est_period(tb1_h0, period);
  }
  else {
    *mp_est_freq = 0.0f;
  }
}

/*
void DsAutotune::analyze_block() {
  using namespace xt::placeholders;

  // USEFUL BUFFER REFS
  // =======================

  // first two quarters of tb0
  auto tb0_q0 = xt::view(m_tbuffer0, xt::range(0, dsat::BLOCK_SIZE));
  auto tb0_q1 =
    xt::view(m_tbuffer0, xt::range(dsat::BLOCK_SIZE, 2 * dsat::BLOCK_SIZE));

  // halves of tb0
  auto tb0_h0 = xt::view(m_tbuffer0, xt::range(0, 2 * dsat::BLOCK_SIZE));
  auto tb0_h1 =
    xt::view(m_tbuffer0, xt::range(2 * dsat::BLOCK_SIZE, 4 * dsat::BLOCK_SIZE));

  // first half of tb1
  auto tb1_h0 = xt::view(m_tbuffer1, xt::range(0, dsat::BLOCK_SIZE));

  // ALGORITHM
  // =======================
  dsat::zero_buffer(tb0_h1);

  // setup short window in tbuffer0
  xt::noalias(tb0_q0) = m_ibuffer0;
  dsat::zero_buffer(tb0_q1);
  // FFT short window => tcbuffer1
  fftwf_execute(m_fft_fwd_short.get());

  // setup long window
  xt::noalias(tb0_q1) = m_ibuffer1;
  // inclusive sum scan x^2 => tbuffer1
  {
    auto tb0_h0_clip = xt::view(tb0_h0, xt::range(0, -1));
    xt::noalias(xt::view(m_tbuffer1, xt::range(1, _))) =
      xt::cumsum(tb0_h0_clip * tb0_h0_clip);
  }
  m_tbuffer1[0] = 0.0f;
  // FFT long window => tcbuffer0
  fftwf_execute(m_fft_fwd_long.get());

  // IFFT(long * conj(short)), => tbuffer0
  xt::noalias(m_tcbuffer0) = m_tcbuffer0 * xt::conj(m_tcbuffer1);
  fftwf_execute(m_fft_rev.get());

  // DF to tbuffer0
  {
    auto a_sq = xt::broadcast(m_tbuffer1[dsat::BLOCK_SIZE], {dsat::BLOCK_SIZE});
    auto b_sq =
      xt::view(m_tbuffer1, xt::range(dsat::BLOCK_SIZE, 2 * dsat::BLOCK_SIZE)) -
      xt::view(m_tbuffer1, xt::range(0, dsat::BLOCK_SIZE));
    auto ab             = tb0_q0;
    xt::noalias(tb0_q0) = a_sq + b_sq - 2 * ab;
  }

  // compute CMNDF to m_tbuffer1
  {
    auto csum        = xt::cumsum(xt::view(tb0_q0, xt::range(0, -1)));
    auto arange      = xt::arange(1.0f, (float) dsat::BLOCK_SIZE);
    auto tb1_h0_clip = xt::view(tb1_h0, xt::range(1, _));

    xt::noalias(tb1_h0_clip) = tb1_h0_clip * arange / csum;
    tb1_h0[0]                = 1.0f;
  }

  // submit estimated period to output
  auto period  = dsat::est_period(tb1_h0, 1024);
  *mp_est_freq = (period != 0)
    ? m_sample_rate / dsat::refine_est_period(tb1_h0, period)
    : 0.0f;
}
*/

static const lvtk::Descriptor<DsAutotune> ds_autotune {DS_AUTOTUNE_URI};