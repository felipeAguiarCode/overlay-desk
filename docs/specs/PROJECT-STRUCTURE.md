# Estrutura de Projeto Recomendada

```text
overlay-desk/
├── CMakeLists.txt
├── CLAUDE.md
├── README.md
├── assets/
├── config/
├── docs/
├── shaders/
│   ├── OverlayDeskVS.hlsl
│   └── OverlayDeskPS.hlsl
├── src/
│   ├── main.cpp
│   ├── Application.cpp
│   ├── Application.h
│   ├── core/
│   │   ├── AppState.h
│   │   ├── Settings.cpp
│   │   ├── Settings.h
│   │   ├── PresetRepository.cpp
│   │   └── PresetRepository.h
│   ├── windowing/
│   │   ├── ControlWindow.cpp
│   │   ├── OverlayWindow.cpp
│   │   ├── TargetWindow.cpp
│   │   └── WindowTracker.cpp
│   ├── capture/
│   │   ├── CaptureSession.cpp
│   │   └── CaptureSession.h
│   ├── graphics/
│   │   ├── D3D11Device.cpp
│   │   ├── Renderer.cpp
│   │   ├── ShaderManager.cpp
│   │   └── RenderResources.cpp
│   ├── ui/
│   │   ├── ControlPanel.cpp
│   │   ├── OverlayPage.cpp
│   │   ├── FiltersPage.cpp
│   │   ├── EffectsPage.cpp
│   │   ├── PresetsPage.cpp
│   │   └── SettingsPage.cpp
│   └── util/
│       ├── Log.cpp
│       ├── Json.cpp
│       └── Win32Helpers.cpp
└── tests/
    ├── settings/
    └── math/
```

## Regra

Não criar uma classe por slider.

Módulos devem refletir responsabilidades reais.
