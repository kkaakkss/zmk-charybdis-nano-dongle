# 왼쪽 연결 끊김 조사 (2026-11-06)

## 확인한 사실과 한계

OSL 인덱스 5/15/25는 `BT_SEL 0/1/2`: PC용 프로필 선택이다.
고정 ZMK의 behavior_bt.c → zmk_ble_prof_select()는 호스트 프로필과 광고 상태를 바꾸며,
좌/우 split bond를 삭제하지 않는다. 이 세 키는 오른쪽 물리 키지만 동작은 중앙 동글에서 실행한다.
현재 설정은 동글 central, L/R peripheral, split 슬롯 2개, 전체 연결 3개, bond 5개다.
딥슬립은 이미 꺼져 있다. 오른쪽은 계속 동작한다는 보고만으로 왼쪽 단독 펌웨어 정지,
저장된 bond 불일치, 무선/전원/컨트롤러 불량 중 하나를 확정할 수 없다.
로컬 코드 검토에서 해당 프로필 키가 왼쪽만 끊도록 하는 설정은 찾지 못했다.
원인 로그 없이 BLE 버전/전력/타이밍 변경이나 bond 자동 삭제를 적용하지 않는다.

## 우선 복구·분리 검사

1. 동글을 USB 연장선으로 USB 3 허브/무선 공유기/금속 케이스에서 떨어뜨리고,
   왼쪽을 동글 가까이 둔다. 왼쪽은 정상 USB 케이블로 전원을 공급해 배터리/스위치 경로와 비교한다.
   USB가 연결된 기기는 스위치 OFF만으로 전원이 완전히 꺼지지 않을 수 있다.
2. 일반 OLED 동글 펌웨어를 설치하고 재시험한다. 이 업데이트가 연결 수정을 보장하지는 않는다.
3. 계속 실패하면 debug 동글과 left-debug를 설치한다. bond는 유지된다.
   각 장치는 Windows에서 별도 COM 포트로 나타난다. 로그용 COM을 각각 열고
   왼쪽 reset 및 OSL 5/15/25 입력 전후 로그를 저장한다. 로깅은 타이밍에 영향을 줄 수 있다.
   재부팅 때 포트가 다시 나타나므로 수집기를 다시 시작해야 할 수 있다.
   PowerShell: `scripts/capture-serial.ps1 -Port COM7 -OutputPath left-log.txt`
   USB로 로그를 받더라도 왼쪽 키 입력은 여전히 BLE를 통해 동글로 전달된다.
4. 로그에 보안/키 불일치가 확인되거나 복구되지 않으면 공식 절차로 전체 split bond를 재설정한다.
   **settings-reset은 저장된 페어링과 설정을 지운다.** 먼저 L/R/동글을 모두 끄고,
   각 기기에 reset UF2를 설치해 실행한 뒤 각자의 정상 펌웨어를 다시 설치한다.
   동글=OLED, 왼쪽=left-recovery, 오른쪽=right-recovery. 모두 가까이 두고 함께 켠다.
   PC Bluetooth로도 썼다면 PC의 이전 키보드 등록도 삭제 후 재연결한다.
   한쪽만 reset하면 반대쪽에 오래된 bond가 남을 수 있다.
5. 재페어링 후에도 USB 전원/근거리에서 왼쪽만 재발하면 판매자에게 로그와 함께 문의한다.
   배터리 모드에서만 실패하는지, USB에서도 실패하는지, 재현 입력/시각/LED 상태,
   동글·오른쪽의 정상 여부를 함께 전달한다. 크리스털/전원/스위치/보드 검사는 판매자 영역이다.

공식 참고: https://zmk.dev/docs/troubleshooting/connection-issues

## 빌드

일상 GitHub Actions는 기존처럼 OLED 한 개만 빌드한다.
선택 빌드는 `scripts/build-local.ps1`에 기존 Workspace/Python/Sdk와
`-Manifest build-diagnostics.yaml -Target 0`을 전달한다.
인덱스 0=OLED debug, 1=왼쪽 debug, 2=왼쪽 정상 복구,
3=오른쪽 정상 복구, 4=공통 settings reset이다.
서로 다른 build 디렉터리를 사용하므로 일반 OLED 빌드를 덮어쓰지 않는다.
진단이 끝나면 정상 OLED/left-recovery로 복귀한다.
