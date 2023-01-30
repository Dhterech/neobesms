#include "suggest.h"

bool e_suggestline_t::containssubdot(u32 subdot) {
    if(
        (subdot >= this->timestamp_start) &&
        (subdot < this->timestamp_end)
    ) {
        return true;
    }
    return false;
}
