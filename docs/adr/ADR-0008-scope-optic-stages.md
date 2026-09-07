# ADR-0008 — Estágios de óptica: zoom e abertura do scope

## Status

Accepted

## Contexto

O pedido é olhar pelo visor de uma luneta: a imagem estufada no centro e um recorte circular
preto e sólido em volta, como quem filma pela ocular de um óptico.

O vinheteamento já produz um escurecimento redondo, e a primeira reação é tratar o scope como
um preset do vinheteamento com os controles no máximo. **Isso não funciona**, por um motivo
que não é estético:

O vinheteamento multiplica a cor e não toca no alpha. Onde a imagem capturada não cobre o
overlay, o alpha é zero e o que está atrás aparece — comportamento correto para as barras de
letterbox, deliberado desde o Milestone 3. Um scope escurecido por vinheteamento herda isso: o
corpo do óptico fica preto onde há imagem e **transparente** onde não há. O desktop aparece
através da parte do overlay que deveria ser um tubo de aço.

Além disso, um vinheteamento é um gradiente e um óptico é uma abertura. A borda do vidro
termina; ela não desvanece ao longo de meio quadro.

Há também a magnificação. Uma luneta mostra menos do mundo, maior. Isso é uma operação de UV,
e nenhum estágio existente faz isso.

## Decisão

Um filtro novo, `scope`, implementado em **dois estágios que abraçam o pipeline** em vez de um
estágio no meio dele:

```text
Captured Texture → Scope Zoom → Jitter UV → Glitch UV → Distortion
                 → Chromatic Aberration → Color Correction → Noise → Scanlines
                 → Vignette → Flicker → Scope Mask → Output
```

A subsequência do ADR-0003 continua intacta, assim como no ADR-0007.

### 1. Zoom antes de tudo

A objetiva magnifica antes de qualquer coisa enxergar a imagem, então `ApplyScopeZoom` é o
primeiro estágio:

```text
uv_source = (uv - 0.5) / magnification + 0.5
```

Ele vem **antes do jitter** de propósito. Deslocar depois do zoom significa que a trepidação é
magnificada junto com a imagem — que é exatamente a razão pela qual um óptico de alta
magnificação é difícil de segurar firme. Deslocar antes do zoom produziria o contrário: quanto
mais zoom, mais estável, que é falso.

Acima de `1.0` as bordas do frame saem de vista. Isso é correto e a abertura cobre o limite de
qualquer maneira.

### 2. Máscara depois de tudo

`ApplyScope` é o último estágio, **depois inclusive do flicker**: o corpo do óptico é metal
opaco, não pisca com a lâmpada e não deixa a borda do Edit Mode aparecer por baixo da parte que
ele cobre.

A máscara é o único estágio do pipeline que escreve **cor e alpha**:

```text
body    = (1 - glass) * intensity
rgb     = lerp(rgb, 0, body)
alpha   = max(alpha, body)
```

`alpha = max(...)` é a parte que resolve o problema do contexto: onde o corpo é sólido, o
overlay é opaco, mesmo em cima de uma barra de letterbox que estaria transparente.

`intensity` esmaece o óptico inteiro em vez de tornar o tubo translúcido — em `1.0` o corpo é
totalmente opaco, em `0.0` a máscara desaparece e o estágio é identidade.

### 3. A curvatura não pertence ao scope

O estufamento continua sendo o filtro de distorção. O scope fornece a abertura e a
magnificação, e nada mais.

**Motivação.** São coisas independentes na prática: dá para querer uma abertura mais larga sem
uma imagem mais plana, ou o contrário. Embutir a curvatura no scope obrigaria a duplicar todo o
`DistortionShape` dentro dele ou a aceitar uma única curvatura fixa. Os presets de óptica
simplesmente ligam os dois filtros.

### 4. Forma como enum, seguindo o ADR-0007

`ScopeShape` tem `circle` e `binocular`. Binóculo são dois círculos que se sobrepõem no meio —
que é o que binóculos produzem opticamente, e não os dois discos separados que o cinema usa.

### 5. Retículo

Opcional, controlado por `reticle` (0 = nenhum). É um retículo duplex: fino no centro para não
cobrir o ponto de mira, engrossando na direção da borda para continuar legível num overlay
pequeno, com marcas de retenção descendo o fio vertical. Desenhado dentro da máscara porque só
faz sentido dentro do vidro.

## Consequências

- O constant buffer vai a 256 bytes. Mesmo argumento do ADR-0007: uma cópia por frame num
  buffer criado uma vez.
- `enabledMask` ganha o bit `FEATURE_SCOPE`. Desligado, os dois estágios são branches coerentes
  e nada mais — verificado: com o scope desligado o frame é idêntico ao passthrough.
- A máscara escrever alpha é uma exceção ao padrão dos outros estágios. É a razão de o filtro
  existir, e está documentada no shader no ponto onde acontece.
- `magnification` não tem contrapartida negativa. Reduzir abaixo de 1 mostraria conteúdo que a
  captura não tem, então o mínimo é `1.0` na UI.

## Verificação

Medido contra um alvo de grade em 600x394, com o restante do pipeline desligado:

| Configuração | Centro | Preto no frame |
|---|---|---|
| Passthrough | 255/255/255 | 2,2% |
| Scope desligado, abertura 0,30 configurada | 255/255/255 | 2,2% |
| Scope ligado, `intensity` 0 | 255/255/255 | 2,2% |
| Scope ligado, abertura 0,60 | 255/255/255 | 41,6% |
| Idem, retículo 100% | 236,9 | 42,6% |
| Binóculo, abertura 0,52 | 255/255/255 | 28,4% |

41,6% bate com a geometria: com `aspect` 600/394, `cornerRadius` = 0,911 e raio = 0,547, o
círculo é cortado em cima e embaixo, sobrando 59,8% de vidro. O binóculo, com dois círculos
sobrepostos, deixa mais vidro e menos preto, como esperado. `intensity` 0 e o filtro desligado
reproduzem o passthrough exatamente.
