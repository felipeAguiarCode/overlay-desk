# Especificação — Configuração e Persistência

## Formato

JSON.

## Local sugerido

```text
%APPDATA%\OverlayDesk\
```

Estrutura:

```text
OverlayDesk/
├── settings.json
├── presets/
│   ├── neutral.json
│   ├── soft-crt.json
│   └── ...
└── logs/
```

## Regras

- escrita atômica;
- versionar schema;
- fallback para defaults;
- arquivo corrompido não deve impedir startup.

## Schema conceitual

```json
{
  "schemaVersion": 1,
  "overlay": {},
  "filters": {},
  "effects": {},
  "render": {},
  "ui": {}
}
```

## Migração

Sempre que schema mudar:

- incrementar `schemaVersion`;
- implementar migration;
- nunca ignorar versão silenciosamente.

## Presets

Preset contém apenas estado visual.

Não deve conter:

- target HWND;
- posição do target;
- PID.

Pode conter:

- filtros;
- efeitos;
- render preferences opcionais;
- `category`, a seção sob a qual o preset aparece na aba Presets.

A `category` é rotulagem, não estado visual: `PresetMatches` a ignora, então mover um preset de
seção nunca faz o painel reportar o look atual como editado. Ela fica no arquivo — e não numa
tabela nome → seção no código — para que renomear um preset não o tire do grupo dele e para que
o usuário possa arquivar os seus onde quiser.

Um arquivo sem o campo, que é o caso de tudo que foi escrito antes de a seção existir, tem a
categoria recuperada pelo nome entre os presets de fábrica; o que não bate com nenhum vai para
`Custom`. Sem isso uma atualização jogaria a instalação inteira em `Custom`, já que o seeding
nunca reescreve um arquivo que já está no disco.

## Auto-save

Configurações globais podem ser salvas ao alterar.

Presets só devem ser modificados por ação explícita do usuário.
