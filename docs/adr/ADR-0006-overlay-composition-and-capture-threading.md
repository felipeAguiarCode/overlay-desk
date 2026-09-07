# ADR-0006 — Composição do overlay e threading da captura

## Status

Accepted

## Contexto

O ADR-0002 define o overlay como uma HWND top-level independente e lista os modos que ela
precisa suportar, mas não especifica **como** essa janela é composta na tela nem em qual
thread os frames capturados são processados.

Essas duas decisões estão acopladas: o mecanismo de composição determina se a janela pode
ter alpha por pixel sem custo de memória extra, e o modelo de threading determina se o
frame capturado pode ir direto para o shader ou precisa de uma cópia.

O ADR-0005 estabelece o budget de RAM e o `CLAUDE.md` proíbe explicitamente cópias
desnecessárias da textura capturada.

## Decisão

### 1. Composição via DirectComposition, não `WS_EX_LAYERED`

A janela overlay é criada como:

```text
style    = WS_POPUP | WS_THICKFRAME
exStyle  = WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW [| WS_EX_TOPMOST]
```

- `WM_NCCALCSIZE` devolve a área cliente igual ao retângulo da janela, o que elimina a
  moldura visual mantendo a máquina de resize do `DefWindowProc` intacta. `WS_THICKFRAME`
  existe apenas para fornecer essa máquina; ele nunca é desenhado.
- A swap chain é criada com `IDXGIFactory2::CreateSwapChainForComposition`
  (`DXGI_FORMAT_B8G8R8A8_UNORM`, `FLIP_DISCARD` com fallback para `FLIP_SEQUENTIAL`,
  2 buffers, `DXGI_ALPHA_MODE_PREMULTIPLIED`) e apresentada por
  `IDCompositionDevice` / `IDCompositionTarget` / `IDCompositionVisual`.

**Motivação.** Swap chains flip-model não compõem corretamente com `WS_EX_LAYERED`, e
`WS_EX_LAYERED` implicaria uma redirection surface do DWM — mais uma superfície do tamanho
da janela (≈8 MB em 1080p) que o ADR-0005 não tem onde acomodar.
`WS_EX_NOREDIRECTIONBITMAP` elimina essa superfície por completo.

### 2. Opacidade global no pixel shader

RF-008 é implementado multiplicando a saída do shader (`rgb *= opacity; a = opacity`), não
com `SetLayeredWindowAttributes`. Custo zero, independente dos controles de intensidade por
módulo, e coerente com o alpha premultiplicado exigido pela swap chain de composição.

### 3. Click-through

Alternado por `WS_EX_TRANSPARENT` combinado com `HTTRANSPARENT` em `WM_NCHITTEST`. Em Play
Mode a janela também recebe `WS_EX_NOACTIVATE`, para não roubar o teclado do jogo. Edit
Mode limpa os dois. Nenhum dos dois depende de `WS_EX_LAYERED`.

### 4. Threading da captura: single-threaded, dirigido por callback

- `winrt::init_apartment(apartment_type::single_threaded)` no `wWinMain`.
- `CreateDispatcherQueueController` com `DQTYPE_THREAD_CURRENT` e **`DQTAT_COM_NONE`** no
  thread do message loop. `DQTAT_COM_STA` é inválido aqui: o apartamento já foi
  inicializado, e pedir ao controller para inicializar outro faz o DispatcherQueue não
  despachar (sintoma observado: a captura inicia, mas `FrameArrived` nunca dispara).
- `Direct3D11CaptureFramePool::Create` — **não** `CreateFreeThreaded`. Assim `FrameArrived`
  é despachado no message loop.
- O render acontece **dentro** do callback, direto da textura do frame pool.

**Consequência principal:** zero cópia, zero staging, zero readback, e nenhum lock em volta
do `ID3D11DeviceContext`. Cumpre literalmente `ARCHITECTURE.md §3`.

### 5. O device D3D11 não pode ser single-threaded

`D3D11_CREATE_DEVICE_SINGLETHREADED` **não** é usado, apesar de todo o desenho da aplicação
acontecer em um único thread. Um device single-threaded recusa `QueryInterface` por
`ID3D11Multithread`, e `Direct3D11CaptureFramePool::Create` pede exatamente essa interface —
falha com `E_NOINTERFACE`. O Windows.Graphics.Capture alimenta o frame pool a partir do
próprio thread produtor, independentemente de onde o evento é despachado.

### 6. Repaint fora do callback reusa a view do último frame

O WGC só entrega um frame quando o conteúdo da fonte muda. Um emulador pausado simplesmente
para de produzir frames.

Para repintar o overlay fora do callback — necessário para animar a moldura de Edit Mode
(AT-006) sobre uma fonte estática — a `CaptureSession` mantém a `ID3D11ShaderResourceView`
do último frame entregue e o renderer desenha a partir dela.

Isso **não** é histórico de frames nem uma cópia: é a mesma textura do frame pool, que só é
sobrescrita quando a fonte produz um novo frame. Nenhuma textura adicional é alocada.

Quando não há captura alguma, o shader desenha um painel escuro translúcido, redesenhado
uma única vez — um overlay ocioso não consome GPU.

### 7. Geometria de render

Fullscreen triangle gerado por `SV_VertexID`: sem vertex buffer, sem index buffer, sem
input layout, e sem a costura diagonal que dois triângulos deixariam visível assim que a
distorção entrar (Milestone 4).

## Alternativas rejeitadas

| Alternativa | Por quê não |
|---|---|
| `WS_EX_LAYERED` + `SetLayeredWindowAttributes` | Incompatível com swap chain flip-model; custa uma redirection surface. |
| `CreateFreeThreaded` + fila de frames | Exigiria sincronizar o device context ou copiar a textura, contrariando `CLAUDE.md`. |
| Copiar o frame para uma textura própria | Uma cópia por frame e mais uma textura do tamanho da fonte — proibido pelo ADR-0005. |
| Child window do emulador | Vetado pelo ADR-0002. |

## Consequências

Positivas:

- alpha por pixel sem superfície de redirecionamento;
- caminho de captura sem cópia e sem thread extra;
- overlay ocioso com custo de GPU nulo.

Negativas:

- depende de DirectComposition (Windows 8+, na prática Windows 10+ pelo WGC);
- o device D3D11 carrega a camada de thread-safety do runtime mesmo sendo usado por um só
  thread — custo pequeno e não negociável, ver item 5;
- a moldura de Edit Mode depende da view do último frame continuar válida; se o frame pool
  for recriado (resize da fonte), ela é invalidada e o overlay volta ao painel ocioso até o
  próximo frame.
