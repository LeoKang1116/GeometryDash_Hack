# Geometry Dash 자동 클리어 프로젝트

이 저장소에는 목적이 다른 두 프로그램을 따로 관리합니다.

## 프로그램

- [`macro-replay/`](macro-replay/README.md): 다른 사람이 녹화한 매크로를 원하는 진행률까지 재생하는 완성된 Geode 모드입니다.
- [`new-map-autoclear/`](new-map-autoclear/README.md): 기본 큐브 테스트 맵의 점프 입력을 탐색하고 원하는 %에서 직접 조작으로 넘기는 실험 버전입니다. 실제 게임 검증은 아직 필요합니다.

두 프로그램은 별도로 빌드하며, 동시 자동 입력을 막는 `common/AutomationOwner.hpp`만 공유합니다. 기존 Target Replay를 빌드하거나 수정할 때는 `macro-replay` 폴더에서 작업합니다.
