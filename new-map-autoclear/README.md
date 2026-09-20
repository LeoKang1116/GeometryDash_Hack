# New Map Autoclear

매크로가 없는 처음 보는 Geometry Dash 레벨을 분석해 입력을 스스로 만드는 자동 클리어 프로그램을 위한 작업 공간입니다.

v0.1.0은 짧은 기본 큐브 맵용 실험 버전입니다. 다른 사람의 매크로 없이 여러 점프 시점을 실제 게임에서 시험합니다. 모든 맵을 해결하는 프로그램은 아니며, 게임 내 성공 여부는 별도 검증이 필요합니다.

## 설치와 사용

Geometry Dash 2.2081 / Geode 5.8.2 이상을 대상으로 빌드한 macOS 패키지입니다.

1. `dist/leokang.new_map_autoclear.geode`를 설치합니다. 기존 Target Replay도 사용한다면 함께 제공되는 v0.1.3으로 업데이트합니다.
2. 게임을 완전히 종료하고 다시 실행합니다.
3. 기본 블록(ID 1)과 가시(ID 8)만 있는 짧은 레벨을 열고 일시정지합니다. 처음에는 에디터에서 만든 간단한 로컬 맵으로 시험하세요.
4. 오른쪽 위 **New Map Auto**를 누릅니다. **Target Replay** 아래에 별도로 표시됩니다.
5. **Manual control at %**에 1~100 정수를 입력하고 **Start**를 누릅니다.
6. 처음부터 자동으로 시도합니다. 죽으면 다른 점프 시점을 시험합니다. 최대 500회 또는 5분입니다.
7. 예를 들어 50을 입력하면 진행률이 처음 50% 이상이 되는 시점에 **YOUR TURN - Auto OFF**가 표시됩니다. 죽이거나 일시정지하지 않고 그대로 직접 플레이합니다.

목표 전에 넘겨받으려면 일시정지 → **New Map Auto → Take Over**를 누르세요. 목표가 100이면 실제 레벨 완료까지 기다립니다. 수동 전환 후에는 자동 입력과 자동 재시도가 모두 꺼집니다. 자동으로 누르던 버튼은 놓으므로, 버튼을 미리 누르고 있었다면 놓았다가 다시 누르세요. 프레임 단위로 전환하므로 정확히 50.00%를 보장하지는 않습니다. 장애물 바로 앞에서 전환하면 직접 점프할 준비가 필요합니다.

## 지원 범위와 두 프로그램의 관계

- 현재는 정상 속도·정상 크기·정방향 중력의 단일 큐브만 지원합니다.
- 회전/크기 변경 없는 기본 블록과 가시만 허용합니다. 다른 오브젝트, 포탈, 장식, 트리거가 있으면 Start에서 설명과 함께 거부합니다. 따라서 Acu 같은 일반 복합 레벨은 현재 지원하지 않습니다.
- Practice, Start Position, Platformer는 지원하지 않습니다. 로컬 맵과 에디터 테스트 플레이는 허용합니다.
- 기존 매크로와 새 모드는 서로 다른 ID로 설치되며, 동일한 장면의 입력 소유권을 공유해 동시에 자동 실행할 수 없게 했습니다. 기존 Target Replay v0.1.2 이하는 소유권 기능이 없으므로 업데이트하거나 비활성화해야 합니다.
- 일시정지 중에는 입력을 보내지 않습니다. Resume은 탐색을 이어가며 Take Over는 탐색을 종료합니다. 사용자 재시작·레벨 종료는 탐색을 취소합니다.
- 목표 도달 전 사망은 성공으로 처리하지 않습니다. 탐색 한도나 시간 초과 시 입력을 놓고 일시정지합니다.
- 해답 후보와 목표 도달 정보는 Geode의 이 모드 저장 폴더에 `last-result.json`으로 저장합니다. 이것은 로컬 맵 데이터가 포함된 진단 파일이며 GDR이나 불러오기 가능한 매크로가 아닙니다.

탐색기는 직전 사망 약 160프레임 안의 바닥 접촉 시점에서 점프 후보를 만듭니다. 더 멀리 간 후보를 우선하되 이전 후보도 보관합니다. 메모리 제한으로 후보는 1024개, 입력은 64번의 점프까지 제한합니다. 해결 가능한 맵도 탐색 예산·후보 간격 때문에 실패할 수 있습니다. 완주 보장은 없습니다.

## 빌드와 검증

저장소 루트에서 실행합니다. 공통 입력 소유권 헤더는 `common/AutomationOwner.hpp`입니다.

```sh
cmake -S new-map-autoclear -B new-map-autoclear/build-tests -DAUTOCLEAR_BUILD_MOD=OFF
cmake --build new-map-autoclear/build-tests --parallel 4
ctest --test-dir new-map-autoclear/build-tests --output-on-failure

cmake -S new-map-autoclear -B new-map-autoclear/build-mac \
  -DGEODE_SDK=/absolute/path/to/geode-sdk \
  -DGEODE_BINDINGS_REPO_PATH=/absolute/path/to/bindings \
  -DCMAKE_BUILD_TYPE=Release '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build new-map-autoclear/build-mac --parallel 4
```

자동 테스트는 % 검증, 목표 통과 직후 입력 차단, 수동 전환 후 재시도 금지, 100% 완료 판정, 사망과 성공 구분, 시도 한도, 후보 되돌아가기 및 독립적인 간단한 테스트 환경에서의 탐색을 확인합니다. **이 테스트는 Geometry Dash 물리나 실제 맵 완주 검증을 대신하지 않습니다.**

실제 게임에서 추가로 확인할 항목:

- 평지 맵에서 50% 수동 전환 후 점프가 되는지, 이후 사망해도 자동으로 재시도하지 않는지.
- 가시 하나/두 번 점프 맵을 스스로 해결하는지, 같은 입력의 결과가 반복 실행에서 일치하는지.
- 수동 Take Over, 일시정지/재개, 재시작, 레벨 종료 후 눌린 버튼이 남지 않는지.
- 각 모드가 자동 실행 중일 때 다른 모드의 Start가 거부되는지, 중지 후 다시 시작할 수 있는지.

구체적인 단계와 합격 조건은 [개발 계획](PLAN.md)에 정리되어 있습니다.

## 현재 구조

```text
new-map-autoclear/
├── CMakeLists.txt
├── mod.json
├── README.md
├── PLAN.md
├── src/main.cpp          # Geode 입출력, 메뉴, 소유권, 수동 전환
├── src/Solver.hpp
├── src/Solver.cpp        # 제한된 입력 후보 탐색
└── tests/SolverTests.cpp
```
