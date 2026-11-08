#!/usr/bin/env python3
"""Executa cenarios e verifica as decisoes com um modelo independente."""
import csv
import os
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parent
BUILD = ROOT / os.environ.get('BUILD_DIR', 'build')
RESULTS = ROOT / 'resultados'

def model(sequence, algo):
    ram, dirty, backing, referenced, last = [], set(), set(), {}, {}
    faults = reads = writes = hand = 0
    victims = []
    for tick, ref in enumerate(sequence, 1):
        page = abs(ref)
        if algo == 'NRU' and (tick - 1) % 4 == 0:
            referenced = dict.fromkeys(ram, False)
        if page not in ram:
            faults += 1
            if len(ram) == 3:
                if algo == 'FIFO': index = 0
                elif algo == 'LRU': index = min(range(len(ram)), key=lambda i: last[ram[i]])
                elif algo == 'NRU': index = next((i for i,p in enumerate(ram) if not referenced[p]), 0)
                elif algo == 'CLOCK':
                    while referenced[ram[hand]]:
                        referenced[ram[hand]] = False
                        hand = (hand + 1) % len(ram)
                    index = hand
                victim = ram.pop(index)
                victims.append(victim)
                if victim in dirty:
                    writes += 1
                    backing.add(victim)
                    dirty.remove(victim)
                hand %= len(ram)
            if page in backing: reads += 1
            ram.append(page)
        referenced[page] = True
        last[page] = tick
        if ref < 0: dirty.add(page)
    return (faults, reads, writes), victims

def run_case(scenario, algo, seed):
    if Path('/tmp/scheduler.sock').exists():
        raise RuntimeError('Termine o ossim anterior antes dos testes')
    name = f'{scenario}_{algo}_{seed}'
    env = dict(os.environ, OSSIM_SEED=str(seed))
    with (RESULTS / (name + '_ossim.log')).open('w') as log:
        server = subprocess.Popen([str(BUILD/'ossim'), '--sched','FIFO', '--vpages','8',
            '--pframes','3','--threshold','1','--vm',algo], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, env=env)
        try:
            for _ in range(100):
                if server.poll() is not None: raise RuntimeError('O simulador terminou ao arrancar')
                if Path('/tmp/scheduler.sock').exists(): break
                time.sleep(.02)
            else: raise TimeoutError('Socket nao criado')
            app = subprocess.run([str(BUILD/'application'), str(ROOT/'cenarios'/f'{scenario}.csv')],
                capture_output=True, text=True, timeout=15)
            (RESULTS/(name+'_application.log')).write_text(app.stdout+app.stderr)
            if app.returncode or 'finished at time' not in app.stdout or 'CPU: 0.000' in app.stdout:
                raise RuntimeError('A aplicacao nao concluiu o cenario')
            time.sleep(.08)
        finally:
            server.terminate()
            server.wait(timeout=5)
    output = (RESULTS/(name+'_ossim.log')).read_text()
    if server.returncode or re.search(r'ERROR|WARNING|AddressSanitizer|runtime error:', output):
        raise AssertionError(output)
    stats = re.findall(r'accesses=(\d+) faults=(\d+) swap-in=(\d+) swap-out=(\d+)', output)
    if len(stats) != 1: raise AssertionError('Faltam estatisticas finais: '+output)
    accesses, faults, reads, writes = map(int,stats[0])
    sequence = [int(re.search(r'\[(-?\d+)\]', line)[1]) for line in
                (ROOT/'cenarios'/f'{scenario}.csv').read_text().splitlines() if line and not line.startswith('#')]
    assert accesses == len(sequence)
    victims = list(map(int,re.findall(r'\[VM\] evict .*?vpn=(\d+)',output)))
    if algo != 'RANDOM':
        expected, expected_victims = model(sequence, algo)
        assert (faults,reads,writes) == expected, (name, stats, expected)
        assert victims == expected_victims, (name,victims,expected_victims)
    else:
        assert len(set(map(abs,sequence))) <= faults <= accesses
        assert 0 <= reads <= faults and 0 <= writes <= faults
    return [scenario,algo,seed,accesses,faults,reads,writes]

def main():
    RESULTS.mkdir(exist_ok=True)
    subprocess.run(['cmake','-S',str(ROOT),'-B',str(BUILD)],check=True)
    subprocess.run(['cmake','--build',str(BUILD)],check=True)
    rows=[]
    for scenario in ['repeticao','localidade','escritas','circular']:
        for algo in ['FIFO','RANDOM','LRU','NRU','CLOCK']:
            for seed in ([1,7,42] if algo=='RANDOM' else [1]):
                rows.append(run_case(scenario,algo,seed))
                print('OK', *rows[-1],flush=True)
    
    random_log = RESULTS/'localidade_RANDOM_42_ossim.log'
    before = re.findall(r'\[VM\] evict .*?vpn=(\d+)', random_log.read_text())
    assert run_case('localidade','RANDOM',42) == next(r for r in rows if r[:3]==['localidade','RANDOM',42])
    assert before == re.findall(r'\[VM\] evict .*?vpn=(\d+)', random_log.read_text())
    for args in [['--vm','INVALIDO'],['--threshold','0'],['--pframes','2','--threshold','3'],['--vpages','61'],['--pframes','999999999999999999']]:
        assert subprocess.run([str(BUILD/'ossim'),*args], capture_output=True, timeout=3).returncode != 0
    with (RESULTS/'resumo.csv').open('w',newline='') as f:
        writer=csv.writer(f)
        writer.writerow(['cenario','algoritmo','seed','acessos','faltas','swap_in','swap_out'])
        writer.writerows(rows)
    print('28 simulacoes verificadas; reproducibilidade RANDOM e argumentos invalidos: OK')

if __name__ == '__main__': main()
