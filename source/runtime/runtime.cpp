#include "datatypes/atomic_load_store.h"
#include "datatypes/hash_table.h"
#include "datatypes/segment_list.h"
#include "vm_context.h"

namespace kf
{
hash_table< int, int > x;
hash_set< int > s;
segment_list< int > ss;

}