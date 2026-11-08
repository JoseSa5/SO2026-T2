# Sistemas Operativos 2026/2027 - Trabalho 2

Simulador de substituicao de paginas com FIFO, RANDOM, LRU, NRU e CLOCK.
O escalonador usado e FIFO. As tarefas opcionais de RR nao foram implementadas.
A aplicacao fornecida (`application.c` e o leitor de CSV) foi preservada.

## Compilar e executar em Linux / DOLOS

```sh
cmake -S . -B build
cmake --build build
bash run.sh --vm LRU --vpages 8 --pframes 3 --threshold 1 --scen cenarios/localidade.csv
```

Trocar LRU por FIFO, RANDOM, NRU ou CLOCK. O script inicia e termina o simulador e
guarda os logs em `resultados/`. Nao deve existir outro ossim a usar o mesmo socket.
Tambem aceita varios argumentos `--scen` para correr aplicacoes em simultaneo.

Para fixar as escolhas do RANDOM no mesmo ambiente:

```sh
OSSIM_SEED=42 bash run.sh --vm RANDOM --vpages 8 --pframes 3 --threshold 1 --scen cenarios/localidade.csv
```

Sem OSSIM_SEED, o RANDOM usa a hora atual. A sequencia de rand() pode variar entre
bibliotecas C, mesmo com a mesma seed.

## Algoritmos e decisoes

- FIFO: remove a pagina carregada ha mais tempo.
- RANDOM: escolhe um indice aleatorio entre as frames ocupadas.
- LRU: remove o menor last_accessed. Usa um relogio logico que avanca uma unidade
  por acesso, para distinguir referencias que ocorrem no mesmo milissegundo.
- NRU: usa um bit R; os bits sao limpos antes dos acessos 1, 5, 9, 13, etc.
  Escolhe a primeira pagina R=0; se todas tiverem R=1 escolhe a mais antiga.
  E a variante de um bit, sem classes por dirty.
- CLOCK: percorre a ordem circular a partir da posicao guardada. R=1 recebe uma
  segunda oportunidade e passa a R=0; a primeira R=0 e removida. Na pesquisa
  seguinte comeca no sucessor da vitima.

As funcoes de substituicao estao em `virtmem.c`. O log `[VM] evict` identifica a
vitima. `[CLOCK] second chance` mostra a passagem do ponteiro, e `[NRU] reset R`
mostra a limpeza periodica. Um HIT nao remove paginas.

`--threshold` tem de estar entre 1 e o numero de frames. Antes de uma falta,
o simulador liberta frames ate ter pelo menos threshold livres, e depois carrega
a pagina pedida. Com threshold=1, todas as frames podem estar ocupadas.

Paginas positivas no CSV sao leituras; negativas sao escritas. As paginas sao
numeradas de 1 ate `--vpages` (maximo 60). Paginas novas lidas ficam limpas;
escritas marcam dirty. So uma vitima dirty gera swap-out. Uma pagina com copia
em swap gera swap-in ao regressar, ficando limpa ate ser escrita de novo.
As operacoes sao simuladas: nao existe leitura/escrita real de dados em disco.

## Testes e resultados

Com Python 3 instalado:

```sh
python3 testar.py
```

Executa 28 simulacoes: quatro cenarios, cinco algoritmos e tres seeds para RANDOM.
Verifica todas as vitimas dos algoritmos deterministas contra um modelo Python,
os totais de faltas/swap, reproducibilidade RANDOM e rejeicao de argumentos invalidos.
Os resultados medidos ficam em `resultados/resumo.csv` e os logs na mesma pasta.

Teste adicional de limites, invariantes, dois processos e libertacao:

```sh
gcc -g -fsanitize=address,undefined -Wall -Wextra -I. tests/memoria.c virtmem.c -o build/teste_memoria
./build/teste_memoria
```

`RELATORIO.md` e a versao editavel do relatorio; `RELATORIO.pdf` e a versao para ler.
Preencher o repositorio GitHub nos dois antes da entrega.

## Criar o ficheiro de entrega

```sh
cmake --build build --target archive
```

Cria `build/entrega.zip` com fontes, cenarios, testes, relatorio e resultados,
sem binarios nem ficheiros temporarios de compilacao.
O repositorio GitHub e o convite aos professores tambem sao exigidos no enunciado.
