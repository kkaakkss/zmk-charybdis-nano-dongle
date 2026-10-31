# Charybdis Nano 35 + USB Dongle

Charybdis Nano 35키를 기존 nRF52840 USB 동글에 연결하는 독립 ZMK 구성입니다.
왼손 18키·오른손 17키/PMW3610은 BLE peripheral, 동글은 BLE central 및 USB keyboard/mouse입니다.
판매자 펌웨어를 분석한 실제 4×5 배선을 사용합니다.

## 키맵

![기본 35키 배열](keymap-drawer/nano35-base.svg)

TOTEM 키맵에서 양쪽 바깥 Shift와 오른쪽 세 번째 엄지를 제외했습니다.
QWERTY/NAV/NUM/OSL 뒤에 Keyball39의 MOUSE/SCROLL/SNIPE를 이식했습니다.

- [7개 레이어 전체 그림](keymap-drawer/nano35.svg)
- [49개 콤보 그림](keymap-drawer/nano35-combos.svg)
- [35키 위치와 토템 매핑](docs/totem35-keymap.md)
- [트랙볼 조작과 감도](docs/trackball-modes.md)

키의 위 표시는 mod-morph의 대체 입력 또는 2x(더블탭), 아래 표시는 홀드 동작입니다.
콤보 위의 L0–L6는 적용 레이어 번호(QWERTY/NAV/NUM/OSL/MOUSE/SCROLL/SNIPE 순)입니다.
투명 키(▽)는 아래 활성 레이어를 따릅니다. 그림은 키 순서를 나타내는 평면 도식이며 케이스 형상은 아닙니다.
원본은 `config/nano35.keymap`, 위치는 실제 펌웨어의 `nano35.dtsi`에서 읽습니다.
키맵을 수정해 push하면 **Draw ZMK keymap** Actions가 그림을 생성하고 기본 브랜치에 갱신합니다.
로컬 생성: `python -m pip install -r requirements-draw.txt` 후 `python scripts/draw-keymap.py`.

## 트랙볼 빠른 조작

기본 레이어에서 공을 움직이면 Mouse가 자동 진입합니다. V+P로 수동 진입할 수도 있습니다.
Mouse에서 E 위치 홀드는 Scroll, R 위치 홀드는 Snipe(1/4 속도), D/F 또는 K/L은 좌/우 클릭,
K+L은 가운데 클릭입니다. 오른손 엄지로 기본 레이어에 복귀합니다.
Snipe의 T 위치는 부트로더 진입입니다. 자세한 타이머·감도 동작은 위 모드 안내를 참고하세요.

## 펌웨어

GitHub의 **Actions → Build Nano35 firmware → 성공한 실행 → Artifacts**에서 받습니다.

| 파일 | 대상 |
|---|---|
| nano35-dongle.uf2 | 기본 USB 동글 |
| nano35-left.uf2 | 왼손 |
| nano35-right.uf2 | 오른손 + PMW3610 |
| nano35-settings-reset.uf2 | 본드/설정 초기화 |
| nano35-dongle-oled.uf2 | 동글 + SH1106 봉고캣·레이어·상태 화면 |
| nano35-dongle-studio.uf2 | 동글 + USB ZMK Studio |

OLED와 Studio는 각각 별도 선택 동글 빌드입니다.
[설치·복구](docs/flashing.md), [OLED 수정 기록](docs/oled-repair.md),
[호환성](docs/compatibility.md), [검증 상태](docs/validation.md)를 먼저 확인하세요.
기본 동글 USB 및 OLED 복구는 사용자 실기기 확인을 받았습니다.
새 트랙볼 모드는 6개 로컬 빌드와 소프트웨어 검사를 통과했으며 실기기 검증은 대기 중입니다.

## 개발과 저장소 관리

- `config/`: 키맵·잠긴 west 의존성
- `boards/`, `snippets/`: 배선·동글·OLED 설정
- `src/`, `dts/`, `zephyr/`: 동글 트랙볼 모듈
- `scripts/`, `tests/`: 빌드·검사·그림 생성
- `keymap-drawer/`: 생성된 YAML/SVG (직접 수정하지 않음)
- `docs/`: 사용·개발 문서

[GitHub 업로드와 자동화](docs/github.md), [로컬 빌드](docs/development.md)를 참고하세요.
UF2, 설치 보관본, 과거 로그는 Git에서 제외하고 로컬에 보존합니다.
봉고캣/OLED 코드의 원본·수정 내역과 MIT 라이선스는
`boards/shields/nano35_status/UPSTREAM.md`, `LICENSE`에 보존되어 있습니다.
