# Target Replay

Geometry Dash에서 기존 매크로를 불러와 **원하는 진행률까지 자동 재생**하는 Geode 모드입니다.
처음 보는 레벨의 입력을 스스로 생성하지 않습니다. 해당 레벨의 매크로 파일이 필요합니다.

## 설치와 사용

1. [Geode](https://geode-sdk.org/)를 설치합니다. 개발 기준은 **Geode 5.8.2 / Geometry Dash 2.2081**입니다.
2. 현재 준비된 `macro-replay/dist/leokang.target_replay.geode`를 Geode의 모드 파일 가져오기로 설치하고 게임을 재시작합니다. 이 패키지는 **Apple Silicon + Intel macOS**용입니다. 기존 버전이 있다면 새 파일을 가져온 뒤 게임을 완전히 다시 시작합니다.
3. 매크로에 해당하는 레벨을 **Normal 모드**로 열고 일시정지합니다.
4. 오른쪽 위 **Target Replay → Choose macro**에서 `.gdr`, `.gdr.json`, `.gdr2` 파일을 선택합니다.
5. **Target %**에 1~100 정수를 입력합니다. **Frame offset**은 처음에는 `0`으로 둡니다.
6. **Start**를 누르면 레벨 처음부터 재생합니다.
7. 도중에 멈추려면 일시정지 → **Target Replay → Stop**을 누릅니다. 일시정지만 하면 이후 Resume으로 계속 재생할 수 있습니다.

목표가 1~99이면 게임이 계산한 진행률이 목표 이상이 되는 업데이트에서 입력을 해제하고 게임의 사망 처리를 호출한 다음 일시정지합니다. 결과 메시지에는 실제 도달한 소수점 진행률을 표시합니다. 목표가 100이면 퍼센트 표시만 보고 중단하지 않고 게임의 실제 완료 처리를 기다립니다. 목표 이전에 죽으면 실패로 표시하고 멈춥니다. 자동 재시도는 하지 않습니다.

**목표 %와 최고 기록은 다릅니다.** 기존 최고 기록이 90%라면 73%에 도달해도 최고 기록을 73%로 낮추지 않습니다. 세이브 파일을 직접 수정하지 않으며 저장은 게임의 정상 처리에 맡깁니다. 게임 업데이트 단위와 텔레포트 등에 의해 73.00% 같은 정확한 소수점 지점이나 특정 정수 표시를 보장하지 않습니다.

이 모드는 BOT/TAS 재생용입니다. 게임 통계에 영향을 줄 수 있고 리더보드 업로드 차단 기능은 없으므로, 실제 플레이 기록과 분리된 로컬 테스트 환경에서 사용하세요.

## 매크로 호환성

- Classic, Normal 모드, 처음부터 재생, **240 TPS** 입력 매크로를 지원합니다.
- GDR1 JSON/MessagePack와 GDR2 version 2 바이너리를 파일 내용으로 구분합니다.
- 매크로의 레벨 ID와 현재 레벨 ID가 같아야 합니다. 이름만 같거나 복사본 ID가 다른 경우 시작하지 않습니다.
- P1/P2 입력을 보존합니다. 구형 xdBot의 입력 인자와 시간 계산 관례는 별도로 처리합니다.
- 다른 TPS, Platformer, Practice, Start Position, 에디터 테스트, 사망 프레임이 포함된 매크로는 지원하지 않습니다.
- **위치·속도 보정 확장 데이터는 재생하지 않습니다.** 확장이 있는 파일은 안내를 표시합니다. 특정 봇의 보정 기능에 의존하는 파일은 읽혀도 완주하지 못할 수 있습니다.
- 레벨 ID가 같아도 레벨 내용, 게임 버전, 랜덤 트리거, 물리 설정, LDM 설정, 다른 모드에 따라 동기화가 어긋날 수 있습니다. 녹화 환경과 맞추세요. TPS를 변경하는 모드나 다른 재생기는 함께 사용하지 마세요.
- 시작부터 일정하게 입력이 어긋날 때만 Frame offset을 조정합니다. 양수는 입력을 그만큼 늦추고, 음수는 앞당깁니다. 범위는 -10~10 물리 프레임입니다. 이것만으로 물리나 레벨 버전 차이를 고칠 수는 없습니다.
- 빈 파일, 손상된 데이터, 잘못된 입력, 32 MiB 초과 파일, 100만 개 초과 입력은 거부합니다.

매크로 예시: [GDMacros](https://www.gdmacros.com/), [XDBot-Macros](https://github.com/iZySCRIPT-rebeellion/XDBot-Macros). 타인의 매크로 파일은 이 저장소에 포함하지 않습니다.

## 소스 빌드

CMake 3.25+, C++23 컴파일러, Geode CLI, Geode SDK v5.8.2와 해당 플랫폼의 SDK 바이너리가 필요합니다. macOS에서는 Xcode Command Line Tools를 사용합니다. SDK 설치는 [Geode 개발 문서](https://docs.geode-sdk.org/)를 참고하세요. Windows 코드는 포함되지만 이 작업에서는 Windows 빌드를 검증하지 않았습니다.

```sh
cmake -S macro-replay -B macro-replay/build-release \
  -DGEODE_SDK=/absolute/path/to/geode-sdk \
  -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build macro-replay/build-release --parallel 4
ctest --test-dir macro-replay/build-release --output-on-failure
```

`GEODE_CLI`가 PATH에 없다면 `-DGEODE_CLI=/absolute/path/to/geode`도 전달합니다. Windows에서는 macOS 아키텍처 옵션을 생략합니다. 결과물은 빌드 폴더의 `leokang.target_replay.geode`입니다. 빌드가 게임 폴더에 자동 설치하지는 않습니다.

이번 빌드에 사용한 바인딩 커밋: `geode-sdk/bindings@7f6c2a75742856de88dad354e576dcff8a28e881`. 동일한 결과를 재현하려면 이 커밋의 체크아웃 경로를 `-DGEODE_BINDINGS_REPO_PATH=/absolute/path/to/bindings`로 지정합니다. SDK와 바인딩은 외부 의존성이며 저장소에 복사하지 않았습니다.

Geode 없이 파서·재생 상태 테스트만 실행하려면:

```sh
cmake -S macro-replay -B macro-replay/build-tests -DTARGET_REPLAY_BUILD_MOD=OFF
cmake --build macro-replay/build-tests --parallel 4
ctest --test-dir macro-replay/build-tests --output-on-failure
```

`replay_tests /absolute/path/to/file.gdr2`로 실제 파일의 읽기 검증도 할 수 있습니다. 이는 **입력 해석 테스트**이며 해당 레벨의 완주 검증은 아닙니다.

## 검증 범위

- Apple Silicon + Intel macOS 공유 라이브러리와 Geode 패키지 빌드.
- GDR1 JSON/MessagePack, GDR2, 손상/잘림, 목표값 검증, 동일 틱 중복 방지, 목표 도달·사망·중지·100% 완료 상태 테스트.
- AddressSanitizer 및 UndefinedBehaviorSanitizer를 사용한 코어 테스트.
- 공개 Zodiac `.gdr.json` 918개 입력과 Living Open `.gdr`/`.gdr2` 각각 478개 입력을 읽는 테스트.
- v0.1.1 실제 macOS 실행 기록에서 Living Open 1.02% 사망까지 입력이 0개 전송된 문제를 확인했습니다. v0.1.2는 macOS에서 인라인되는 `processCommands` 대신 실제 호출되는 `processQueuedButtons`에서 입력을 전송하고, 2.2081의 반 틱 진행 카운터를 240 TPS 프레임으로 변환합니다.
- v0.1.2로 Living Open을 실제 완주했습니다. 다른 레벨의 완주는 매크로와 녹화 환경에 따라 달라질 수 있습니다.

게임에서 확인할 항목: 간단한 레벨의 정상 매크로로 목표 1%/50%/100%, 목표 전 사망, 중간 일시정지와 재개, Stop 후 수동 조작, 다른 레벨 매크로 거부, 게임 종료 후 기록 유지 여부.

## 기술 참고

- [Geode 바인딩](https://github.com/geode-sdk/bindings): 게임 함수와 구조체.
- [GDReplayFormat](https://github.com/maxnut/GDReplayFormat/tree/gdr2): GDR2 와이어 형식. 실제 구현의 문자열은 길이 접두사 방식이며, duration/TPS는 IEEE 부동소수점입니다.
- [구형 xdBot](https://github.com/ZiLko/xdBot): GDR1 입력과 시간 계산 관례 확인용. 해당 봇의 코드를 라이브러리로 포함하지 않습니다.
- [nlohmann/json v3.12.0](https://github.com/nlohmann/json/tree/v3.12.0): JSON/MessagePack 파싱, MIT 라이선스. CMake가 가져옵니다.
