#include <algorithm>
#include <array>
#include <complex>
#include <cstdlib>

#include <fftw3.h>
#include <lv2/core/lv2.h>
#include <lv2/core/lv2_util.h>

#include <xtensor.hpp>

#include <lvtk/lvtk.hpp>
#include <lvtk/plugin.hpp>

#include "util.hpp"
#include <xtensor/containers/xstorage.hpp>
#include <xtensor/core/xeval.hpp>
#include <xtensor/core/xmath.hpp>
#include <xtensor/core/xtensor_forward.hpp>
#include <xtensor/generators/xbuilder.hpp>
#include <xtensor/misc/xcomplex.hpp>
#include <xtensor/views/xbroadcast.hpp>
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

  // real buffers. m_buffer0 stores input blocks before they are processed.
  xt::xtensor<float, 1> m_buffer0;

  // complex buffers.
  xt::xtensor<std::complex<float>, 1> m_cbuffer0;

  // Index into buffer0 for input.
  size_t m_buffer0_idx;
  // FFTW plan for transforming m_buffer0 to m_cbuffer0.
  dsat::fftwf_plan_ptr m_fft_fwd;
  dsat::fftwf_plan_ptr m_fft_rev;
};

DsAutotune::DsAutotune(const lvtk::Args& args) :
  Plugin(args),
  m_sample_rate(args.sample_rate),
  m_buffer0({dsat::BLOCK_SIZE * 2}, 0),
  m_cbuffer0({dsat::BLOCK_SIZE * 2}, 0),
  m_buffer0_idx(0),
  m_fft_fwd(
    dsat::wrap_fftwf(fftwf_plan_dft_r2c_1d(
      dsat::BLOCK_SIZE * 2, m_buffer0.data(),
      reinterpret_cast<fftwf_complex*>(m_cbuffer0.data()), FFTW_ESTIMATE))),
  m_fft_rev(
    dsat::wrap_fftwf(fftwf_plan_dft_c2r_1d(
      dsat::BLOCK_SIZE * 2, reinterpret_cast<fftwf_complex*>(m_cbuffer0.data()),
      m_buffer0.data(), FFTW_ESTIMATE))) {}

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
  xt::noalias(m_buffer0) = xt::zeros<float>(m_buffer0.shape());
  m_buffer0_idx          = 0;

  *mp_est_freq = 0.0f;
}

void DsAutotune::run(uint32_t n_samples) {
  this->fill_block(mp_input, mp_input + n_samples);
  // for now, this serves as an identity function
  std::copy(mp_input, mp_input + n_samples, mp_output);
}

void DsAutotune::fill_block(const float* begin, const float* end) {
  using namespace xt::placeholders;
  while (begin != end) {
    size_t n_to_block_end = dsat::BLOCK_SIZE - m_buffer0_idx;
    size_t n_to_input_end = end - begin;
    if (n_to_input_end >= n_to_block_end) {
      // copy and fill remainder of block
      xt::noalias(
        xt::view(m_buffer0, xt::range(m_buffer0_idx, dsat::BLOCK_SIZE))) =
        dsat::to_xt_adaptor(begin, n_to_block_end);

      // push pointer, reset block counter
      begin += n_to_block_end;
      m_buffer0_idx = 0;

      // swap buffers
      this->analyze_block();
    }
    else {
      // not enough samples to fill this block, so fill what we can now
      xt::noalias(
        xt::view(
          m_buffer0,
          xt::range(m_buffer0_idx, m_buffer0_idx + n_to_input_end))) =
        dsat::to_xt_adaptor(begin, n_to_input_end);

      // push counters
      begin += n_to_input_end;
      m_buffer0_idx += n_to_input_end;
    }
  }
}

void DsAutotune::analyze_block() {
  using namespace xt::placeholders;
}

static const lvtk::Descriptor<DsAutotune> ds_autotune {DS_AUTOTUNE_URI};