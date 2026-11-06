// vtdot.vv vd, vs2, vs1, vm   (Zvtakum)
//   vd[0] = RN_SEW( vd[0] + sum_{i<vl, ativo} vs2[i] * vs1[i] )
// Convencao de acumulador como no vmacc (vd e lido e escrito). Acumulo em precisao ampliada
// (mantissa de 64 bits, produtos exatos) e UM unico arredondamento final para SEW bits.
// Como as reducoes do RVV: vstart deve ser 0; se vl == 0, vd nao e escrito.
require_vector(true);
require(P.VU.vsew == e8 || P.VU.vsew == e16 || P.VU.vsew == e32);
require_align(insn.rs2(), P.VU.vflmul);
require_align(insn.rs1(), P.VU.vflmul);
require(P.VU.vstart->read() == 0);
{
  const reg_t vl = P.VU.vl->read();
  const reg_t rd_num = insn.rd(), rs1_num = insn.rs1(), rs2_num = insn.rs2();
#define VTDOT_BODY(T, N)                                                              \
  {                                                                                   \
    TakumAcc acc = takum_acc_init(P.VU.elt<T>(rd_num, 0), N);                         \
    for (reg_t i = 0; i < vl; ++i) {                                                  \
      VI_LOOP_ELEMENT_SKIP();                                                         \
      takum_acc_fma(acc, P.VU.elt<T>(rs2_num, i), P.VU.elt<T>(rs1_num, i), N);        \
    }                                                                                 \
    if (vl > 0) P.VU.elt<T>(rd_num, 0, true) = (T)takum_acc_result(acc, N);           \
  }
  if (P.VU.vsew == e8)       VTDOT_BODY(uint8_t, 8)
  else if (P.VU.vsew == e16) VTDOT_BODY(uint16_t, 16)
  else                       VTDOT_BODY(uint32_t, 32)
#undef VTDOT_BODY
  VECTOR_END;
}
