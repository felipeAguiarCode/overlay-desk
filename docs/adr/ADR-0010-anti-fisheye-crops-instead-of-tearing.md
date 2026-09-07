# ADR-0010 — O anti-fisheye corta em vez de abrir furo

## Status

Accepted. Corrige a fórmula que o ADR-0007 §2 fixou, para a metade negativa apenas.

## Contexto

O ADR-0007 estabeleceu que cada forma de distorção escala a coordenada centrada por

```text
(1 + k*t) / (1 + k)
```

e afirmou que dividir por `(1 + k)` ancora os extremos em si mesmos, o que garantiria que
"nenhuma quantidade de distorção jogue conteúdo para fora da tela nem abra uma falha
transparente na borda". A `FILTERS-AND-EFFECTS.md` §2.1 repete a promessa e o `USAGE.md`
diz ao usuário que "os cantos ficam ancorados em qualquer valor, então nada é cortado".

**A promessa não era cumprida na metade negativa.** Isso só foi descoberto quando o pipeline
ganhou um harness capaz de renderizar o pixel shader fora da tela e medir o resultado.

Para `k > 0` a expressão fica em `[0, 1]`: o mapa lê para dentro do quadro e não tem como sair
dele. Para `k < 0` o mesmo não vale.

- O fator fica **acima de 1 em todo ponto interno**, porque com `k` negativo o numerador é
  maior que o denominador sempre que `t < 1`. Em `amount -0.5` já basta para que as bordas de
  cima e de baixo leiam de fora do quadro capturado, e o resultado eram cunhas transparentes
  ali.
- Em `amount -1` o mapa alcançava **1,089 do raio do canto** (máximo em `r = 0,816`), medido.
  Tudo além de 1,0 não tem fonte para amostrar, e voltava transparente. Pior: nessa faixa a
  função deixa de ser monotônica, ou seja, a imagem se dobrava sobre si mesma antes de sumir.
- A ancoragem também é apenas radial. Num overlay que não é quadrado, ancorar no raio do canto
  não ancora o meio das arestas, que ficam a uma distância menor do centro — e é exatamente ali
  que os furos apareciam primeiro.

A raiz do problema não é aritmética, é geométrica: **um pincushion precisa de conteúdo que está
fora do quadro capturado.** Ele puxa as bordas para dentro, e o que deveria ocupar o lugar delas
nunca foi capturado. Não existe fórmula que produza um pincushion honesto e ao mesmo tempo
preencha o quadro sem inventar pixels.

## Decisão

A metade positiva **não muda**. Continua sendo `(1 + k*t) / (1 + k)`, byte por byte, e nenhum
preset de fisheye tem o seu visual alterado.

A metade negativa passa a ser

```text
1 / (1 - k*t)
```

que vale 1 no centro e `1/(1 + |k|)` no extremo. As bordas ficam magnificadas em relação ao
meio — que é o pinch — e o fator **nunca passa de 1**, então o mapa é incapaz de sair do quadro,
qualquer que seja a forma, a quantidade e a proporção do overlay.

Em `k = 0` as duas metades valem exatamente 1, então a continuidade em zero que o AT-008 exige
continua saindo da álgebra, sem caso especial.

**O preço, e ele é deliberado: os extremos deixam de ser ancorados em si mesmos na metade
negativa.** O anti-fisheye corta. Em `amount -1` o quadro mostra os dois terços centrais da
fonte, simetricamente ao fisheye, que magnifica o centro em 1,5x.

Cortar um pouco é a escolha honesta contra mostrar um buraco. É também o que qualquer software
de correção de lente faz: `crop to fill` é o padrão justamente porque a alternativa é uma borda
preta com o formato da distorção.

## Consequências

- A `FILTERS-AND-EFFECTS.md` §2.1 e o `USAGE.md` passam a dizer que a ancoragem dos extremos
  vale para a metade positiva, e que a negativa corta. A promessa que sobra — **nunca abrir
  falha transparente** — passa a ser verdadeira pela primeira vez, para as duas metades.
- Os cinco presets da família anti-fisheye (`Anti-Fisheye Light`, `Anti-Fisheye Strong`,
  `Lens Correction`, `Anti-Fisheye Horizontal`, `Anti-Fisheye Corners`) mudam de aparência.
  Eles tinham furos; agora não têm. Os valores não foram retunados: o que eles pediam sempre foi
  um pinch daquela intensidade, e agora é isso que recebem.
- Nenhum preset de fisheye muda.
- `tests/shader/ShaderPipelineTests.cpp` passa a exigir, para as cinco formas e oito
  quantidades entre -1 e +1, que **nenhum pixel** volte transparente quando a fonte e o overlay
  compartilham a proporção. É a promessa deste ADR, medida em vez de afirmada.
- O mesmo teste mede a curvatura em vez do deslocamento, porque com o corte a escala global
  deixou de distinguir as duas metades: uma linha reta arqueia para fora do centro no fisheye e
  para dentro no anti-fisheye, e é só isso que separa os dois de forma robusta.

## Alternativas descartadas

**Inverter exatamente o mapa positivo.** Conceitualmente é o mais bonito — o anti-fisheye
desfazendo o fisheye — e a inversa de uma bijeção `[0,1] → [0,1]` também é uma. Mas isso vale
para o raio, não para o retângulo: medido em 4:3, a inversa manda o meio da aresta superior
para 0,597 em unidades onde o quadro acaba em 0,5. Furo de novo, além de custar iterações de
Newton por pixel.

**Ancorar na borda do quadro (norma de Chebyshev) em vez de no raio do canto.** Resolve o furo
e mantém a ancoragem, mas troca as isolinhas circulares por retangulares, o que muda o caráter
do filtro nas duas metades — e a metade positiva não tem defeito nenhum para justificar isso.

**Deixar como estava e documentar o furo.** A borda transparente aparece a partir de
`amount -0.1` num overlay 16:9. Documentar não a torna útil.
