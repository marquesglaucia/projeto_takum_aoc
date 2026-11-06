// vtsub.vv vd, vs2, vs1, vm   (Zvtakum)   vd[i] = vs2[i] - vs1[i]   (mesma ordem do vfsub.vv: vs2 - vs1)
require_vector(true);
require(P.VU.vsew == e8 || P.VU.vsew == e16 || P.VU.vsew == e32);
VI_VV_LOOP
({
  if (sew == e8)       vd = (int8_t) takum_sub((uint8_t) vs2, (uint8_t) vs1, 8);
  else if (sew == e16) vd = (int16_t)takum_sub((uint16_t)vs2, (uint16_t)vs1, 16);
  else                 vd = (int32_t)takum_sub((uint32_t)vs2, (uint32_t)vs1, 32);
})
