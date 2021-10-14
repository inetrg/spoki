#define CAF_TEST_NO_MAIN

#include "caf/test/unit_test_impl.hpp"

#include "caf/init_global_meta_objects.hpp"
#include "caf/io/middleman.hpp"

#include "core-test.hpp"

#include "spoki/all.hpp"

int main(int argc, char** argv) {
  using namespace caf;
  init_global_meta_objects<id_block::spoki_core>();
  init_global_meta_objects<id_block::spoki_core_test>();
  io::middleman::init_global_meta_objects();
  core::init_global_meta_objects();
  return test::main(argc, argv);
}