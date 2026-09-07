# ADR-0005 — Budget de performance

## Status

Accepted

## Objetivo

Evitar que Overlay Desk se torne mais pesado que o emulador.

## Decisões

- processamento na GPU;
- sem histórico de frames no MVP;
- sem readback por frame;
- sem alocação de GPU por frame;
- shader principal fundido;
- render pausável;
- FPS configurável.

## Meta operacional

Para 1080p:

- RAM ideal abaixo de 150 MB;
- investigar acima de 250 MB;
- memória estável em 60 minutos.

## Observação

Os números são budgets de engenharia e devem ser validados empiricamente em builds Release.
