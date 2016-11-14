#include <algorithm>
#include <chrono>
#include <iomanip>
#include <string>
#include <cstdio>
#include <boost/timer/timer.hpp>

#include "marian.h"
#include "mnist.h"
#include "trainer.h"
#include "models/feedforward.h"

#include "tensors/tensor.h"
#include "tensors/tensor_gpu.h"
#include "tensors/tensor_allocator.h"

using namespace marian;
using namespace keywords;
using namespace data;
using namespace models;

struct ParametersGRU {
  Expr Uz, Wz, bz;
  Expr Ur, Wr, br;
  Expr Uh, Wh, bh;
};

Expr CellGRU(Expr input, Expr state,
             const ParametersGRU& p) {

  Expr z = dot(input, p.Wz) + dot(state, p.Uz);
  if(p.bz)
    z += p.bz;
  z = logit(z);

  Expr r = dot(input, p.Wr) + dot(state, p.Ur);
  if(p.br)
    r += p.br;
  r = logit(r);

  Expr h = dot(input, p.Wh) + dot(state, p.Uh) * r;
  if(p.bh)
    h += p.bh;
  h = tanh(h);

  // constant 1 in (1-z)*h+z*s
  auto one = state->graph()->ones(shape=state->shape());

  auto output = (one - z) * h + z * state;
  return output;
}

std::vector<Expr> GRU(Expr start,
                      const std::vector<Expr>& inputs,
                      const ParametersGRU& params) {
  std::vector<Expr> outputs;
  auto state = start;
  for(auto input : inputs) {
    state = CellGRU(input, state, params);
    outputs.push_back(state);
  }
  return outputs;
}

void construct(ExpressionGraphPtr g, size_t length) {
  g->clear();

  int dim_i = 500;
  int dim_h = 1024;

  ParametersGRU pGRU;
  pGRU.Uz = g->param("Uz", {dim_h, dim_h}, init=uniform());
  pGRU.Wz = g->param("Wz", {dim_i, dim_h}, init=uniform());
  //pGRU.bz = nullptr; // g->param("bz", {1, dim_h}, init=zeros);

  pGRU.Ur = g->param("Ur", {dim_h, dim_h}, init=uniform());
  pGRU.Wr = g->param("Wr", {dim_i, dim_h}, init=uniform());
  //pGRU.br = nullptr; //g->param("br", {1, dim_h}, init=zeros);

  pGRU.Uh = g->param("Uh", {dim_h, dim_h}, init=uniform());
  pGRU.Wh = g->param("Wh", {dim_i, dim_h}, init=uniform());
  //pGRU.bh = nullptr; //g->param("bh", {1, dim_h}, init=zeros);

  auto start = name(g->zeros(shape={whatevs, dim_h}), "s_0");
  std::vector<Expr> inputs;
  for(int i = 0; i < length; ++i) {
    auto x = name(g->input(shape={whatevs, dim_i}),
                  "x_" + std::to_string(i));
    inputs.push_back(x);
  }

  auto outputs = GRU(start, inputs, pGRU);
}

int main(int argc, char** argv) {
  auto g = New<ExpressionGraph>();

  boost::timer::cpu_timer timer;
  for(int i = 1; i <= 1000; ++i) {
    size_t length = rand() % 40 + 10; // random int from [10,50]
    g->clear();
    construct(g, length);

    BatchPtr batch(new Batch());
    for(int j = 0; j < length; ++j)
      batch->push_back(Input({80, 500}));

    g->forward(batch);
    if(i % 100 == 0)
      std::cout << i << std::endl;
  }
  std::cout << std::endl;
  std::cout << timer.format(5, "%ws") << std::endl;

  return 0;
}
