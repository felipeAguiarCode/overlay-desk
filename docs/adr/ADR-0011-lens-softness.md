# ADR-0011 — Lens softness: a lente deixa de ser só geometria

## Status

Accepted

## Contexto

O filtro de distorção descreve **onde** a luz vai parar. Ele não descreve nada sobre a
qualidade dela, e por isso todo preset de fisheye até aqui era o mesmo warp geométrico de uma
imagem digitalmente nítida em toda a área — o que nenhuma lente do mundo produz.

Uma lente real não resolve igual em todo o campo. Ela é nítida no meio e se desfaz nos cantos,
e quanto mais aberta, mais cedo isso começa. É o que separa uma foto ultra-wide de um
screenshot deformado, e é a coisa mais notável que faltava para os presets de fisheye
convencerem: com dez presets variando `amount` e `shape`, o que os distinguia era essencialmente
a cor.

O `Nintendo64Preset` já registrava a lacuna no próprio comentário: *"There is no blur stage in
the pipeline, so this leans on soft scanlines and a lifted gamma to reach the same
low-contrast, milky feel rather than pretending otherwise."*

## Decisão

Um filtro novo, `LensSoftness`, com um parâmetro além do `enabled`/`intensity`:

```text
center   raio da zona que permanece nítida, como fração da distância até o canto
```

Fora de `center` o borrão cresce **quadraticamente** até a borda. Ramp linear deixaria o meio do
quadro levemente mole em toda parte; o quadrado mantém o centro genuinamente intocado, que é
como uma curva de MTF se comporta.

**Seis amostras num anel**, não uma espiral. Isto é um amolecimento de baixa frequência e não um
bokeh — a cobertura extra que uma espiral compra não sobreviveria à exibição, e o bloom já paga
doze amostras.

### Posição: antes do bloom

O desfoque acontece **na lente**, o florescimento acontece **no sensor atrás dela**. O que
floresce é o borrão de um ponto brilhante, não o contrário. Inserir entre a amostragem e o bloom
preserva a ordem relativa de tudo que já existia — a subsequência congelada do ADR-0003, e as
posições do ADR-0007 e do ADR-0009.

### O helper de amostragem passa a reportar cobertura

`SampleSourceRgb` virou `SampleSourceTap`, devolvendo a cor em `.rgb` e 1 ou 0 em `.w`.

A distinção importa e é o motivo da mudança: bloom e edge glow **somam** luz, então uma amostra
fora do quadro pode contribuir zero sem consequência. A lens softness **calcula uma média**, e
uma amostra fora do quadro contribuindo zero puxaria a média para o preto — as bordas do
letterbox ganhariam uma orla escura. Quem faz média tem de ponderar pela cobertura.

## Consequências

- O constant buffer **não cresce**: os dois campos novos couberam no padding que o ADR-0009
  deixou. Continua em 352 bytes, e o `static_assert` não muda.
- O `enabledMask` vai a 20 bits.
- É o terceiro estágio cujo custo não é constante por pixel, junto com bloom e edge glow. Seis
  amostras contra as doze do bloom, e zero quando desligado, como qualquer outro.
- O `Nintendo 64` passa a usar o estágio com `center` quase em zero: o encoder daquele console
  borrava o quadro inteiro, não só os cantos. O comentário que registrava a lacuna foi
  substituído pelo que o preset agora de fato faz.
- Doze presets novos dependem dele para se distinguirem uns dos outros.

## Alternativas descartadas

**Um passe de blur dedicado.** Mesma objeção do bloom no ADR-0009 §2: uma textura do tamanho da
janela que o ADR-0005 não comporta. Com seis amostras num raio pequeno a diferença não se paga.

**Parâmetro do filtro de distorção.** Tentador, porque na prática andam juntos. Mas o
amolecimento é útil sem distorção nenhuma — uma lente barata é mole nos cantos mesmo sem
barrilete — e o `Nintendo 64` usa exatamente essa combinação. Enterrá-lo dentro da distorção
tornaria esse caso inexprimível.

**Blur gaussiano separável de verdade.** Precisa de duas passadas, logo de um render target.
Mesma recusa.
