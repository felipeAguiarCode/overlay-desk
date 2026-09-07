# ADR-0009 — Estágios de sensor: bloom, false colour, edge glow, shimmer, rolling shutter, lens dirt e scan sweep

## Status

Accepted

## Contexto

O conjunto de filtros e efeitos até aqui descreve **monitores**: tubos, máscaras de fósforo,
lentes de câmera. O produto ganhou uma segunda direção — as ópticas e câmeras táticas do
ADR-0008 e da família de presets que veio com ele — e essa direção esbarrou em cinco coisas que
os módulos existentes não conseguem expressar de jeito nenhum.

1. **Térmico não é uma tinta.** O preset `Thermal` aproxima uma vista infravermelha com
   `saturation = 0` e uma tinta âmbar. Isso produz um monocromático *em uma cor*, que é o que a
   tinta faz por definição (ADR-0007 §3). Uma paleta térmica de verdade é um **mapeamento de
   luminância para cor**: `white hot` é monotônico mas `ironbow` não é — ele passa por preto,
   roxo, vermelho, laranja e branco, e a matiz sobe e desce. Nenhuma multiplicação produz isso.
   `black hot` precisa inverter a luminância antes de mapear, o que também não é multiplicação.

2. **Um intensificador de imagem floresce.** O que identifica visão noturna não é o verde — é o
   halo violento em torno de qualquer fonte de luz. Um tubo saturado espalha carga, e o
   resultado é um borrão brilhante que engole detalhe. Sem isso, um preset verde lê como um
   filtro de cor aplicado a um jogo, não como uma vista através de um tubo.

3. **Vistas de sensor desenham contornos.** As "vistas magnéticas" e HUDs de sensor de jogos
   táticos destacam silhuetas com uma linha brilhante. Isso é detecção de borda — uma derivada
   espacial da imagem. Nenhum estágio atual lê mais de um ponto da fonte por canal.

4. **Camuflagem óptica é refração.** Um campo que dobra a luz atrás dele é uma perturbação de
   UV contínua e orgânica. O `jitter` desloca o quadro inteiro rigidamente e o `glitch` desloca
   bandas; nenhum dos dois ondula.

5. **Compressão digital não se parece com dano analógico.** O feed de um drone quebra em
   macroblocos e degraus de banding. O `glitch` atual modela fita e sinal de RF: bandas
   deslocadas horizontalmente e separação de canais. São artefatos de tecnologias diferentes e
   um não substitui o outro.

Somam-se dois artefatos que faltavam para uma câmera corporal ser convincente: o **rolling
shutter**, que inclina a imagem quando quem carrega a câmera se move, e a **sujeira na lente**,
que é o que mais denuncia um equipamento que vive preso a um colete.

A pergunta arquitetural é a mesma do ADR-0007: isso cabe no ADR-0003 ou o contradiz.

## Decisão

### 1. Sete estágios novos, a ordem relativa dos anteriores intacta

O pipeline passa a ter dezoito posições:

```text
 1. Scope zoom            ADR-0008
 2. Jitter UV             ADR-0007
 3. Shimmer UV            NOVO
 4. Rolling shutter UV    NOVO
 5. Glitch UV             ADR-0003
 6. Distortion            ADR-0003
 7. Chromatic aberration + amostragem   ADR-0003
 8. Bloom                 NOVO
 9. Color correction      ADR-0003
10. False colour          NOVO
11. Edge glow             NOVO
12. Noise                 ADR-0007
13. Scanlines             ADR-0003
14. Vignette              ADR-0003
15. Lens dirt             NOVO
16. Scan sweep            NOVO
17. Flicker               ADR-0007
18. Scope mask            ADR-0008
```

A subsequência que o ADR-0003 congelou — `Glitch UV → Distortion → Chromatic Aberration →
Color Correction → Scanlines → Vignette` — mantém a ordem relativa exata. As posições que o
ADR-0007 fixou (jitter antes do glitch, noise entre correção de cor e scanlines, flicker
depois da vinheta) também. Os estágios novos ocupam lacunas; nenhum reordena o que já existia.

**Posição de cada estágio novo.**

- **Shimmer** e **rolling shutter** entram no grupo de UV que o jitter abriu, antes do glitch.
  A sequência descreve uma cadeia física: o jitter diz onde a câmera está, o shimmer diz o que
  o ar (ou o campo de camuflagem) fez com a luz no caminho, o rolling shutter diz como o sensor
  leu essa luz linha a linha, e só então o glitch diz o que aconteceu com o sinal já formado.
- **Bloom** vem logo depois da amostragem e **antes de qualquer gradação**. O florescimento
  acontece no sensor ou no tubo, com a luz que chegou — não com a imagem já corrigida. A
  consequência prática importa: com visão noturna, o bloom é gerado em monocromático e só
  depois recebe o fósforo, que é a ordem certa. Se viesse depois, o halo teria a cor da paleta
  em vez de ser colorido por ela.
- **False colour** vem imediatamente depois da correção de cor, pelo mesmo motivo que a tinta
  vem depois da saturação (ADR-0007 §3): a receita que funciona é dessaturar e então mapear.
  Mapear uma imagem ainda colorida usa uma luminância que a saturação vai contaminar.
- **Edge glow** vem depois da false colour porque o contorno é um elemento de interface do
  sensor desenhado *sobre* a imagem — ele não deve ser recolorido pela paleta térmica. Antes do
  noise, porque o grão do sensor incide sobre tudo que o sensor mostra, contorno inclusive.
- **Lens dirt** vem depois da vinheta e antes do flicker. A sujeira está no elemento frontal,
  na frente de tudo que a lente produziu, mas atrás do brilho da lâmpada que ilumina a cena —
  e atrás do corpo do óptico, que continua sendo o último a escrever.
- **Scan sweep** vem junto, logo antes do flicker: é uma barra de luz do próprio display, então
  respira com ele.

### 2. Bloom é aproximação single-pass, não um passe dedicado

O ADR-0003 permite um passe dedicado quando o efeito exige blur, mas condiciona isso a um ADR.
Este ADR **recusa** o passe dedicado.

Um blur separável de verdade custaria pelo menos uma textura do tamanho da janela e duas
passadas de leitura e escrita sobre ela. O ADR-0005 contabiliza 8 MB por render target a 1080p
e 33 MB a 4K, e recusa explicitamente texturas extras. Trocar isso por um halo mais bonito não
se paga.

O bloom é implementado como **uma espiral de amostras da textura de origem** em torno do UV já
distorcido, mantendo apenas a parte que passa do `threshold` e somando o resultado. É glow, não
gaussiana. O halo tem estrutura visível se o `radius` for empurrado ao máximo, e isso é o
comportamento aceito, não um defeito a corrigir depois.

Fica registrado para quem vier: **se um dia o bloom precisar de qualidade fotográfica, isso é
um passe novo e um ADR novo, não um ajuste na contagem de amostras deste.**

### 3. Variantes continuam dentro do módulo

Seguindo a regra 2 do ADR-0007:

- `GlitchStyle { analog, digital }` é um enum **dentro** do glitch, não um módulo de compressão
  separado. São duas formas do mesmo evento — um estouro de sinal danificado — e separá-las
  duplicaria `enabled`, `intensity` e `frequency` e permitiria o estado sem sentido de dois
  glitches simultâneos.
- `ScopeShape` ganha `quadTube`, os quatro círculos sobrepostos de um NVG panorâmico, ao lado
  de `circle` e `binocular`. É a mesma abertura com outra geometria.
- `posterize` entra como parâmetro da correção de cor, não como filtro. É uma quantização
  tonal, exatamente a categoria dos outros parâmetros daquele módulo. Em `0` é identidade.

### 4. False colour é módulo próprio, não parâmetro da correção de cor

É a única das adições que poderia razoavelmente ter virado parâmetro de um módulo existente, e
não virou. Três razões:

1. A correção de cor é um conjunto de operações **neutras em valores neutros**, e todas são
   ajustes de uma imagem que continua sendo ela mesma. Uma paleta substitui a cor da imagem
   inteira. São categorias diferentes.
2. Ela precisa do próprio `enabled`/`intensity` para permitir a mistura parcial — `intensity`
   interpola entre a imagem gradada e a paleta, que é como se faz um térmico "suave".
3. A ordem exige: a paleta tem que vir **depois** de toda a correção de cor, inclusive da tinta.
   Como parâmetro, ela estaria dentro de um módulo cuja ordem interna já está fixada.

### 5. `schemaVersion` não é incrementado

Todos os campos novos são aditivos e nascem desligados. A leitura tolerante já em uso
(`SettingsJson.h`) faz campo ausente cair no default, então um `settings.json` ou um preset
escrito por uma versão anterior carrega sem perda.

Incrementar teria o efeito oposto do pretendido: a leitura recusa um `schemaVersion` maior do
que conhece e cai inteira nos defaults (`Settings.cpp`). Subir para `2` significaria que
qualquer build anterior, ao encontrar o arquivo novo, descartaria **toda** a configuração do
usuário em vez de apenas ignorar os campos que não entende.

É o mesmo caminho que o ADR-0007 e o ADR-0008 seguiram ao acrescentar tinta, scope, noise,
flicker e jitter, e a regra de migração da `CONFIGURATION.md` continua valendo para o que ela
de fato governa: mudança **incompatível** de formato.

### 6. Nenhum estágio novo guarda frame anterior

Shimmer, rolling shutter, scan sweep e lens dirt são funções puras de `g_time` e da
coordenada. Bloom e edge glow leem a textura de origem em pontos adicionais **do frame atual**.
Nada é acumulado entre frames, nada é lido de volta para a CPU, nenhum render target
intermediário é criado. A proibição de histórico de frames do `CLAUDE.md`, do ADR-0003 e do
ADR-0005 continua respeitada.

## Consequências

- O constant buffer cresce de 256 para 352 bytes (22 registradores). Ele continua sendo escrito uma vez por frame
  com `Map(WRITE_DISCARD)` num buffer criado uma única vez, então o custo é o tamanho da cópia.
  O `static_assert` que amarra o tamanho ao layout do `Common.hlsli` sobe junto.
- O `enabledMask` vai de 12 para 19 bits. Cada estágio desligado custa um branch coerente.
- **Bloom e edge glow são os dois primeiros estágios cujo custo não é constante por pixel.**
  Todos os anteriores fazem aritmética sobre uma amostra; estes fazem amostras adicionais — a
  espiral do bloom e a cruz do edge glow. Desligados custam zero, como qualquer outro, mas
  ligados são a única coisa no shader que pode mover o frame time de forma mensurável. O
  orçamento de RAM do ADR-0005 não muda, porque nenhuma textura nova existe; o que muda é
  trabalho de amostragem, e ele é medido e registrado na `MANUAL-TESTS.md`.
- A lógica de letterbox, que estava contida em `SampleSource`, passa a ser compartilhada: o
  helper `SampleSourceRgb` é extraído para que bloom e edge glow amostrem pela mesma regra.
  Amostra fora do conteúdo contribui zero — sem isso o halo vazaria para dentro das barras.
- As páginas de Filters e Effects ficam longas o bastante para que a ordem dos cards importe.
  Elas seguem a ordem do pipeline, como já seguiam.
- A regra de estilo HLSL do ADR-0007 continua obrigatória: **uma variável de resultado e um
  único `return` por função**, porque `fxc` roda com `/WX` e o caminho `/Od` do build Debug
  reporta `return` antecipado dentro de branch como X4000.

## Alternativas descartadas

**LUT em textura para as paletas térmicas.** Seria uma amostra em vez de um punhado de `lerp`,
e permitiria paletas arbitrárias. Custa um recurso de GPU permanente, um arquivo para carregar
ou um blob para embutir, e um caminho de inicialização que pode falhar. As cinco paletas que o
produto quer são descritíveis por poucos pontos de controle, e um gradiente avaliado em ALU não
ocupa memória nem pode faltar em disco.

**Detecção de borda por diferença de luminância entre pixels vizinhos do resultado já
processado.** Mais barato — não custaria amostras novas. Mas o resultado já processado carrega
scanlines, grão e vinheta, e a detecção acharia as bordas *desses* padrões em vez das do
conteúdo. O contorno tem que sair da imagem da fonte.

**Rolling shutter como parâmetro do jitter.** Os dois deslocam a imagem no tempo, mas o jitter
desloca o quadro inteiro rigidamente e o rolling shutter cisalha em função da linha. Fundi-los
daria um módulo com dois comportamentos e um `intensity` ambíguo.

**Sujeira de lente vinda de uma textura.** Mesma objeção do grão no ADR-0007: um recurso de GPU
permanente e um padrão que se repete, em troca de uma economia por pixel que o hash não cobra.
