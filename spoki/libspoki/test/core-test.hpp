#include "caf/test/bdd_dsl.hpp"
#include "caf/test/io_dsl.hpp"

#include "spoki/all.hpp"

using dummy_placeholder
  = caf::typed_actor<caf::replies_to<int32_t>::with<int32_t>>;

CAF_BEGIN_TYPE_ID_BLOCK(spoki_core_test, spoki::detail::spoki_core_end)

  CAF_ADD_TYPE_ID(spoki_core_test, (dummy_placeholder))

CAF_END_TYPE_ID_BLOCK(spoki_core_test)
