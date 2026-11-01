# 호환성 분석과 구현 계획

## 근거와 확정 범위

분석 대상은 `kkaakkss/zmk-sofle-dongle`, `bdimitrako/zmk-config-charybdis-nano`,
`victorlucachi/zmk-keyboards-charybdis`, `tangbonze/zmk-config-charybdis_nano` 및
판매자 `20260307-nano35.zip`이다. 기존 체크아웃은 수정하지 않는다.

| 항목 | 소스에서 확인한 사양 | 확인 범위 |
|---|---|---|
| 동글 MCU | nice_nano_v2, nRF52840 | Sofle build.yaml 기준; 실물 보드/부트로더 확인 필요 |
| 동글 OLED | SH1106, I2C 0x3c, SDA P0.17, SCL P0.20 | 기존 overlay와 동일 |
| OLED geometry | width 128, height 64, segment-offset 2, multiplex 63 | 2026-11-01 끝열 잔점 대응; 표준 중앙128열 배치에 맞춤. 실물 확인 필요 |
| 전원 제어 | nice_nano_v2 EXT_POWER P0.13, active high | 보드 DTS; 실제 클론의 스위칭 회로 확인 필요 |
| Nano MCU | SuperMini nRF52840/nice_nano_v2 호환 | Nano README 및 판매자 UF2 family ID 일치 |
| 키 | 좌 18 + 우 17 = 35 | 3x5 + 좌 엄지 3/우 엄지 2 |
| 매트릭스 | row2col, 양쪽 모두 4행/5열 | 판매자 UF2의 GPIO 배열과 변환 테이블 확인 |
| 행 | Pro Micro 21,18,5,4 → P0.31,P1.15,P0.24,P0.22 | 판매자 양쪽 바이너리에서 확인 |
| 열 | Pro Micro 19,20,10,6,7 → P0.02,P0.29,P0.09,P1.00,P0.11 | 판매자 양쪽 바이너리에서 확인 |
| PMW3610 | 오른손, SCK P0.08, SDIO P0.17, CS P0.20, MOTION P0.06 | 소스와 바이너리 SPI/GPIO 상수 교차 확인 |

**중요: 이 제품은 참고 저장소의 4x6 배선과 다르다.** 판매자 바이너리의 GPIO 배열을
ZMK의 `kscan_gpio` 구조체(12바이트, gpio spec + 원래 인덱스)와 대조해 4x5를 복원했다.
양쪽 테이블 모두 같은 행/열/인덱스다. row2col은 열의 pull-down과 행의 active-high 설정에 부합한다.

35키 lookup은 왼손 주소 0x4e438, 오른손 0x5da48에서 일치한다.
각 행의 첫 3줄은 `[1..5,10..6]`, `[11..15,20..16]`, `[21..25,30..26]`이고
마지막 행은 `[0,0,31,32,33,0,0,0,35,34]`다. 값은 keymap position+1이다.
각 transform 구조체는 rows=4, columns=10, length=40이며 오른손 offset=5/왼손=0이다.
따라서 좌 본문은 로컬 열 0..4, 우 본문은 4..0, 좌 엄지는 (3,2),(3,3),(3,4),
우 엄지는 (3,4),(3,3)로 구현했다. 참고 저장소의 41키/6열 매핑을 재사용하지 않는다.

## 소프트웨어 선택

- ZMK: 공식 v0.3-branch의 `acfd8e5ea76cf23ad1c9b6b99848f97a95224257` 고정.
- Zephyr: 해당 manifest의 3.5 (`dacab4875df7`), 판매자 바이너리 문자열과 일치.
- PMW3610: badjeff `14d39b7a6944d1014544a2382707b14724c22f49` 고정.
  `pixart,pmw3610`, `irq-gpios`, `CONFIG_PMW3610_*` API를 함께 사용한다.
- 최신 Zephyr 내장 드라이버와 외부 드라이버를 혼용하지 않는다.
- SDK: 로컬 0.16.8. 0.17.4는 Zephyr 3.5 picolibc lock 선언과 충돌하여 제외했다.
- CI: zmk-build-arm:3.5의 amd64 이미지 digest 고정. 새 stable 이미지로 자동 이동하지 않는다.
- Nano 참고 README에 기록된 특정 센서의 RES_STEP 읽기 초기화 실패를 피하는 이전 드라이버다.
  이 설명은 참고 저장소의 관측이며, 첨부 바이너리만으로 센서 레지스터 동작까지 확정하지 않는다.

## 구조와 단계

1. 동글이 BLE Central로 두 Peripheral을 연결하고 키맵과 USB HID keyboard/mouse 보고를 처리한다.
2. 왼손은 키 스캔만, 오른손은 키 스캔과 PMW3610 input-split 인덱스 0을 전송한다.
3. 동글 listener는 물리 센서 대신 split 입력 0을 구독한다. 오른손을 Central로 설정하지 않는다.
4. 최소 빌드를 먼저 실물 검증한다. OLED와 Studio는 별도 UF2로 테스트한다.
5. 전원 절약/자동 수면, 고급 포인터 프로세서, 사용자별 키맵은 기본 통신 검증 후 추가한다.

ZMK 0.3에서 split BLE는 ZMK_BLE에 의존한다. 따라서 ZMK_BLE를 끄지 않으며,
현재 토템 키맵에서는 저장 본드를 좌우 2개와 호스트 프로필 3개로 총 5개 예약한다.
동시 연결은 좌우 2개와 활성 호스트 1개, 총 3개로 유지한다. USB가 연결되면 기본
출력은 USB다. PC는 동글의 BLE 호스트 프로필과 페어링할 필요가 없다.

## 판매자 바이너리

ZIP에는 왼손/오른손/settings_reset 3개 UF2가 있으며 동글 UF2는 없다.
모두 family ID `0xada52840`, 시작 주소 `0x26000`이고 UF2 블록 magic을 검사했다.
`Zephyr OS v3.5.0`, `dacab487`, `V&Z-Nano35` 문자열을 확인했다.
오른손에는 BLE central/scan 경로 문자열이 있어 기존 오른손 Central 구성과 부합한다.
단, 문자열은 역할/전체 설정의 완전한 증명은 아니다.

오른손 payload의 주소 0x612b8에는 PSEL 0x40008/0x50011/0x60011이 있다.
주소 0x5d194의 device name은 `gpio@50000000`이다. 이를 가리키는 GPIO spec 중
0x612c4 및 0x612f4에는 pin20/active-low, 0x612d4에는 pin6/active-low/pull-up이
있어 참고 배선과 부합한다. ELF 심벌이 없으므로 정적 데이터 해석의 한계를 유지한다.

원본 ZIP과 UF2는 첨부 파일/환경 보관 경로에 두며 공개 저장소에 포함하지 않는다.
SHA-256은 로컬 `docs/archive/vendor-firmware.json`에 기록한다. 센서 방향, 전원 지연, RF 안정성,
매트릭스의 물리 키 위치는 실물 키 테스트로 최종 확인해야 한다.
