#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
if (($# == 0)); then
    echo 'Uso: bash run.sh --vm FIFO|RANDOM|LRU|NRU|CLOCK --vpages 8 --pframes 3 --threshold 1 --scen cenarios/localidade.csv'
    exit 0
fi
scenarios=()
args=()
while (($#)); do
    if [[ $1 == --scen ]]; then
        (($# >= 2)) || { echo 'Falta o ficheiro de cenario' >&2; exit 1; }
        [[ -f $2 ]] || { echo "Ficheiro inexistente: $2" >&2; exit 1; }
        scenarios+=("$2")
        shift 2
    else
        args+=("$1")
        shift
    fi
done
((${#scenarios[@]})) || { echo 'Indique --scen ficheiro.csv' >&2; exit 1; }
build=${BUILD_DIR:-build}
[[ -x $build/ossim && -x $build/application ]] || { echo 'Compile primeiro: cmake -S . -B build && cmake --build build' >&2; exit 1; }
[[ ! -S /tmp/scheduler.sock ]] || { echo 'Ja existe um socket do simulador. Termine o ossim anterior antes de iniciar outro.' >&2; exit 1; }
mkdir -p resultados
stamp=$(date +%Y%m%d-%H%M%S)-$$
server_log="resultados/ossim-$stamp.log"
app_log="resultados/application-$stamp.log"
children=()
server=''
cleanup() {
    for pid in "${children[@]}"; do kill "$pid" 2>/dev/null || true; done
    if [[ -n $server ]]; then
        kill -TERM "$server" 2>/dev/null || true
        wait "$server" 2>/dev/null || true
    fi
}
trap cleanup EXIT
trap 'exit 130' INT TERM
"$build/ossim" "${args[@]}" >"$server_log" 2>&1 &
server=$!
for ((i=0;i<100;i++)); do
    kill -0 "$server" 2>/dev/null || { cat "$server_log"; exit 1; }
    [[ -S /tmp/scheduler.sock ]] && break
    sleep .05
done
[[ -S /tmp/scheduler.sock ]] || { echo 'Timeout ao iniciar o ossim' >&2; exit 1; }
for file in "${scenarios[@]}"; do
    timeout 120 "$build/application" "$file" >>"$app_log" 2>&1 &
    children+=("$!")
done
for pid in "${children[@]}"; do wait "$pid" || { cat "$app_log"; exit 1; }; done
children=()
sleep .1
kill -TERM "$server"
wait "$server"
server=''
grep -q 'stats:' "$server_log" || { cat "$server_log"; exit 1; }
if grep -Eq 'ERROR|WARNING|failed|invalid' "$server_log" "$app_log"; then
    cat "$server_log" "$app_log"
    exit 1
fi
cat "$app_log"
grep 'stats:' "$server_log"
echo "Logs: $server_log e $app_log"
