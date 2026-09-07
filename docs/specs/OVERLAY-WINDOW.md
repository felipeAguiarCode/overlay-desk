# Especificação — Overlay Window

## Objetivo

A janela Overlay é o produto visual principal.

Ela deve permanecer funcional independentemente da janela de controle.

## Propriedades

```text
enabled
x
y
width
height
resizable
lockPosition
lockSize
lockAspectRatio
aspectRatio
alwaysOnTop
clickThrough
opacity
fullscreen
monitorId
```

## Resize

### Free Resize

- resizable = true
- lockAspectRatio = false

### Proportional Resize

- resizable = true
- lockAspectRatio = true

### Fixed

- lockSize = true

## Position

`lockPosition=true` impede movimento manual.

## Edit Mode

Atalho recomendado:

```text
Ctrl + Shift + O
```

Opcional e configurável.

Ao entrar:

- click-through OFF;
- borda visível;
- handles de resize;
- título discreto;
- overlay continua renderizando.

Ao sair:

- restaurar flags anteriores.

## Match Target

Ação:

```text
Match Target Window
```

Copia bounds úteis do target para o overlay.

## Fullscreen

O usuário escolhe monitor.

O overlay ocupa a área do monitor selecionado.

## Always-on-top

Usar mecanismo nativo Win32.

## Click-through

Quando ligado, input de mouse deve passar para a janela abaixo.

Teclado não deve ser capturado pelo overlay durante Play Mode.

Exceção transitória: enquanto o menu rápido está aberto (ADR-0012), o click-through e o
`WS_EX_NOACTIVATE` ficam suspensos, porque um menu que não se pode clicar não é um menu. Os
dois voltam ao que as settings dizem assim que o menu fecha. Fora desse intervalo a regra
acima vale sem ressalva — em Play Mode o overlay não vê tecla nenhuma, e é por isso que o
menu rápido também é alcançável por um atalho global.

## Opacidade

Controla composição global da janela.

Não substituir os controles de intensidade individuais.

## DPI

Processo deve ser DPI aware.

Resize e posição devem permanecer corretos em múltiplos monitores com escalas diferentes.

## Source e Overlay

Source e overlay têm dimensões independentes.

Não presumir:

```text
source width == overlay width
source height == overlay height
```

O renderer deve escalar source para overlay.
