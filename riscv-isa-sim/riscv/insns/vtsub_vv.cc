#include "../../takum_decoder.h"

VI_VV_LOOP({
    if (sew == 8) {
        vd = takum_subtracao_T8(vs1, vs2);
    }
    else if (sew == 16) {
        vd = takum_subtracao_T16(vs1, vs2);
    }
    else if (sew == 32) {
        vd = takum_subtracao_T32(vs1, vs2);
    }
    else {
        require(0);
    }
})