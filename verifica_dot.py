#!/usr/bin/env python3
"""Valida o ACUMULADOR do takum_dot bit a bit contra um modelo independente.

Modelo (a especificacao do vtdot.vv):
    acc := init
    para cada i:  acc := RN64( acc + a[i]*b[i] )      # produto exato; RN64 = mantissa de 64 bits, nearest-even
Usa racionais exatos (fractions.Fraction): nao depende de long double nem do x87.
Entrada: dot_vectors.txt gerado por test_dot (valores em hexfloat de long double).
"""
import re, sys
from fractions import Fraction

def parse(s):
    m = re.match(r'(-?)0x([0-9a-f]+)(?:\.([0-9a-f]*))?p([+-]\d+)$', s)
    sg, i, f, e = m.groups(); f = f or ''
    v = Fraction(int(i + f, 16), 16 ** len(f)) * (Fraction(2) ** int(e))
    return -v if sg else v

def rn64(x):
    """Arredonda x (Fraction) para 64 bits de mantissa, nearest-even."""
    if x == 0: return Fraction(0)
    sg = x < 0; x = abs(x)
    e = x.numerator.bit_length() - x.denominator.bit_length()
    while Fraction(2) ** e > x: e -= 1
    while Fraction(2) ** (e + 1) <= x: e += 1
    scaled = x / (Fraction(2) ** (e - 63))
    q = scaled.numerator // scaled.denominator
    rem = scaled - q
    if rem > Fraction(1, 2) or (rem == Fraction(1, 2) and (q & 1)): q += 1
    r = Fraction(q) * (Fraction(2) ** (e - 63))
    return -r if sg else r

def main(path='dot_vectors.txt'):
    lines = open(path).read().split('\n'); i = 0; casos = bad = 0
    while i < len(lines):
        if not lines[i].startswith('CASE'): i += 1; continue
        _, n, ln, init = lines[i].split(); ln = int(ln); acc = parse(init); i += 1
        for _ in range(ln):
            a, b = lines[i].split(); i += 1
            acc = rn64(acc + parse(a) * parse(b))
        esperado = parse(lines[i].split()[1]); i += 1
        casos += 1
        if acc != esperado:
            bad += 1
            if bad <= 5: print(f'DIVERGE: T{n} len={ln}: modelo={float(acc):.6g} cpp={float(esperado):.6g}')
    print(f'[7] acumulador vs modelo Python (racionais exatos): {casos} casos, {bad} divergencias')
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main(*sys.argv[1:]))
