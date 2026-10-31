# OLED 복원과 USB 정지 경로 수정 — 2026-10-31

## 실기기 기준 (사용자 보고)

- 기존 기본 동글 펌웨어: 연결 및 USB 인식 정상.
- 기존 OLED 펌웨어: 흰 노이즈, USB 장치 인식 실패 반복.
- 기본 펌웨어 복귀: USB 오류가 사라짐.
- 완전한 전원 차단: 화면 노이즈가 사라짐.

위 항목은 수정 전 관측이다. 이후 사용자가 수정한 OLED 화면의 복구를 확인했다.
새 트랙볼 모드는 별도 실기기 확인 대상이다. [현재 검증 상태](validation.md)를 참고한다.
디스플레이 RAM은 MCU 재시작만으로 지워지지 않을 수 있다. 기본 펌웨어는 OLED를
다시 초기화하지 않으므로 화면 잔류만으로 기본 펌웨어의 오류라고 판단하지 않는다.

## 확인한 코드 경로

고정 Zephyr `dacab4875df72109b96cc8977547a0dc04875bcd`의
`modules/lvgl/Kconfig.memory` 기본 렌더 버퍼는 전체의 10%, static bits-per-pixel은
32다. 원본의 CMake 생성 설정에서도 10%/32비트/시스템 작업 큐임을 확인했다.
기존 선택 snippet은 이 값을 재정의하지 않는다. 기존 폭 129, 높이 64에서는
`lvgl.c` 계산 결과 버퍼는 3300바이트지만 LVGL에 전달하는 pixel count는 825다.

SH1106의 `SCREEN_INFO_MONO_VTILED`는 `lvgl_rounder_cb_mono()`에서 갱신 높이를
8줄 단위로 올린다. 전체 폭에서는 825 / 129 = 6줄밖에 들어가지 않는다.
고정 LVGL `8a6a2d1d29d17d1e4bdc94c243c146a39d635fdd`의
`src/core/lv_refr.c:get_max_row()`는 맞는 높이를 찾지 못해 0을 반환한다.
호출부의 `for (...; ...; row += max_row)`는 이때 진행하지 못한다.
ZMK 기본 화면은 시스템 작업 큐를 사용하므로 이 경로는 USB/BLE 작업도 막을 수 있다.
이것은 소스와 함수 회귀 검사로 확인한 정지 경로이며, 실물 USB 트레이스나 fault 로그를
수집한 것은 아니다. 해당 실물에서 유일한 원인이라고 확정하지 않는다.

## 적용한 수정

- 논리 화면을 128×64로 수정. 129는 표시 영역의 폭으로 사용하지 않는다.
  기존 SDA P0.17 / SCL P0.20, 주소 0x3c, segment-offset 1, 방향/반전은 유지.
  화면이 한 칸 치우치는 경우에만 실기기 근거로 segment-offset을 조정한다.
- 1비트 색상/저장 형식, 100% 버퍼를 명시: pixel count 8192, 실제 버퍼 1024바이트.
- 디스플레이 전용 작업 큐, 스택 4096바이트, LVGL 힙 8192바이트, tick 20ms.
- 원본 Sofle의 `englmaxi/zmk-dongle-display@v0.3` 의존 화면을 로컬
  `nano35_status` shield로 복원. 봉고캣·레이어·출력·modifier·잠금·배터리 위젯과
  이미지/라이선스를 재사용한다. 원본 전체 DYA/Prospector 의존성은 필요하지 않다.
- 원본 봉고캣과 HID 위젯이 초기화 NULL 이벤트를 역참조하던 두 getter를 수정.
  이 문제는 복원 시 방지한 추가 버그이며 기존 기본 OLED 화면의 원인으로 혼동하지 않는다.
- OLED 복원 당시 BASE / NAV / POINTER를 표시했다. 현재는 7개 키맵 레이어 이름을 표시한다.
- Windows에서 CMake 경로의 `\U` escape 오류가 나지 않게 build.py의 `-D` 경로를
  슬래시로 변환하고, `scripts/build-local.ps1` 실행기를 추가했다.
  공백이 있는 Desktop 소스 경로는 공백 없는 작업 공간의 NTFS junction을 통해 참조한다.
  위젯은 LVGL을 먼저 include해 Windows 드라이브 문자 C와 ZMK C 키 매크로의 충돌도 피한다.

## 회귀 검사와 한계

`scripts/test-display.py --workspace <west-workspace>`는 실제 설치된 고정 LVGL의
`get_max_row()`와 Zephyr의 `lvgl_rounder_cb_mono()`, 로컬 두 위젯 getter의 C 코드를
추출하여 컴파일한다. 원래 설정의 0줄 반환을 재현하고 수정된 버퍼의 1024개 크기
조합에서 진행 가능한 높이를 확인한다. NULL 초기화/실제 이벤트/다른 이벤트도 검사한다.
디스플레이 메타데이터·이벤트 cast·현재 상태 API만 테스트 더블이다.

기본 실행은 호스트 GCC/Clang + UBSan이다. Windows 호스트 컴파일러가 없을 때는
`--arm-gcc <SDK>/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc.exe`로 ARM C를 컴파일해
Unicorn 2.1.4로 실행한다. 이 모드에는 UBSan이 없으며, pyelftools와 Unicorn이 필요하다.
이 테스트는 USB, I2C 전기 신호, BLE 무선, OLED 실물 동작을 에뮬레이션하지 않는다.

생성 DTS/Kconfig/UF2 검사는 `scripts/verify-builds.py`로 한다. 이전 2026-10-21
검증 결과와 이번 실행 결과는 별도 기록으로 보존한다.

## 새 UF2 실물 확인 순서

1. 정상 작동한 기존 기본 동글 UF2를 복구용으로 보관한다. 반쪽 펌웨어와 본드는
   그대로 두고 동글 OLED만 교체한다. 이번 변경은 본드 초기화를 요구하지 않는다.
2. USB와 보조 전원/배터리가 있다면 모두 분리해 화면이 꺼지는 것을 확인한 뒤 연결한다.
3. 부팅 직후 USB keyboard/mouse 인식과 흰 노이즈 유무를 확인한다.
4. QWERTY 화면, 타이핑 시 봉고캣, 레이어 변경 중 화면 이름 전환을 확인한다.
5. 키 입력과 트랙볼을 동시에 사용하고, drag/release와 10분 이상 USB 유지 여부를 확인한다.
6. USB 재삽입, PC 절전/복귀, OLED idle blank/입력 후 복귀를 확인한다.
7. USB 인식 실패가 재발하면 기본 UF2로 복구한다. 표시 방향/offset 문제와 USB 재시작은
   별도로 기록하고, 가능하면 Windows 장치 오류 코드 및 MCU 로그를 남긴다.

