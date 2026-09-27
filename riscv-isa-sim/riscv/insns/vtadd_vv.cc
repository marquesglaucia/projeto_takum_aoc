#include "../../takum_decoder.cpp"

VI_VV_LOOP({
    if (sew == 8) {
        TakumFormat opA = decodificar_T8(vs1);
        TakumFormat opB = decodificar_T8(vs2);
        TakumFormat resultado = takum_adicao(opA, opB);
        vd = codificar_T8(resultado); 
    } 
    else if (sew == 16) {
        TakumFormat opA = decodificar_T16(vs1);
        TakumFormat opB = decodificar_T16(vs2);
        TakumFormat resultado = takum_adicao(opA, opB);
        vd = codificar_T16(resultado);
    } 
    else if (sew == 32) {
        TakumFormat opA = decodificar_T32(vs1);
        TakumFormat opB = decodificar_T32(vs2);
        TakumFormat resultado = takum_adicao(opA, opB);
        vd = codificar_T32(resultado);
    }
    else {
        require(0); 
    }
})