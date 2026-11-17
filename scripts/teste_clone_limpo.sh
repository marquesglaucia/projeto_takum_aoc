#!/usr/bin/env bash
# Teste de clone limpo: clona o repositorio (com submodulos) numa pasta nova e roda tudo.
# Rode antes de cada checkpoint, DEPOIS de commitar (so o que esta commitado entra no clone).
#
#   scripts/teste_clone_limpo.sh                       # clona este repositorio local
#   scripts/teste_clone_limpo.sh URL_OU_CAMINHO        # ex.: https://github.com/marquesglaucia/projeto_takum_aoc.git
#   SPIKE=/caminho/spike scripts/teste_clone_limpo.sh  # tambem roda spike-test e analise-isa
#
# Falha (codigo != 0) se algo quebrar ou se o build deixar arquivos nao ignorados no clone.
set -euo pipefail
ORIGEM="${1:-$(git rev-parse --show-toplevel)}"
DEST="$(mktemp -d)/clone"
trap 'echo "(clone em $DEST)"' EXIT
ok() { echo "[OK] $*"; }
falha() { echo "[FALHOU] $*" >&2; exit 1; }

git clone -q --recurse-submodules "$ORIGEM" "$DEST" || falha "git clone --recurse-submodules"
cd "$DEST"
[ -f third_party/libtakum/takum.h ] || falha "submodulo libtakum vazio no clone"
ok "clone com submodulos ($(git rev-parse --short HEAD))"

pip install -q -r requirements.txt >/dev/null 2>&1 || python3 -c "import numpy, matplotlib" || falha "dependencias Python"
make all >make_all.log 2>&1 || { tail -20 make_all.log; falha "make all"; }
[ "$(grep -c 'RESULTADO: TUDO OK' make_all.log)" -ge 2 ] || falha "make all: faltam as mensagens 'TUDO OK' (test e dot)"
grep -q 'falhas: 0' make_all.log || falha "make all: sem 'falhas: 0'"
! grep -E 'falhas: [1-9]|FALHA' make_all.log >/dev/null || falha "make all: ha falhas nos testes"
ok "make all (test, integracao, dot, spmv)"

if [ -n "${SPIKE:-}" ]; then
  make spike-test SPIKE="$SPIKE" 2>&1 | grep -q 'Zvtakum OK' || falha "make spike-test"
  ok "make spike-test"
  make analise-isa SPIKE="$SPIKE" >analise.log 2>&1 || { tail -20 analise.log; falha "make analise-isa"; }
  ok "make analise-isa"
else
  echo "[--] spike-test e analise-isa pulados (defina SPIKE=/caminho/do/spike)"
fi

sujeira="$(git status --porcelain --untracked-files=all | grep -v -E '^ M (grafico_.*\.png)$' | grep -v -E 'make_all.log|analise.log' || true)"
[ -z "$sujeira" ] || { echo "$sujeira"; falha "o build deixou arquivos nao ignorados (veja .gitignore)"; }
ok "nenhum arquivo gerado fora do .gitignore (graficos regravados por 'make spmv' sao esperados)"
echo "CLONE LIMPO: TUDO OK"
