// vncvt.t.t vd, vs2, vm   (Zvtakum)   estreitamento Takum 2*SEW -> SEW, com arredondamento:
//   SEW=8 : T16 -> T8      SEW=16 : T32 -> T16      (SEW=32 e ilegal: nao ha T64)
require_vector(true);
require(P.VU.vsew == e8 || P.VU.vsew == e16);
VI_CHECK_SDS(false);
VI_LOOP_BASE
  if (sew == e8) P.VU.elt<uint8_t>(rd_num, i, true)  = (uint8_t) takum_cvt(P.VU.elt<uint16_t>(rs2_num, i), 16, 8);
  else           P.VU.elt<uint16_t>(rd_num, i, true) = (uint16_t)takum_cvt(P.VU.elt<uint32_t>(rs2_num, i), 32, 16);
VI_LOOP_END
