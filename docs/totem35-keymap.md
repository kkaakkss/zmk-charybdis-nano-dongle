# TOTEM → Charybdis Nano 35키

후속으로 [Mouse/Scroll/Snipe 모드](trackball-modes.md)를 추가했다. 아래 클릭 키 없음 설명은 토템만 이식한 이전 단계 기록이다.

참조: `kk-totem-dongle-main/config/totem.keymap`.
shield 폴더의 기본 예제 키맵이 아닌 현재 config 키맵을 사용했습니다. 토템 저장소는 수정하지 않았습니다.

스크린샷의 X 표시대로 토템의 0부터 시작하는 키 번호 **20, 31, 37**을 모든 레이어에서 제외했습니다.
각각 왼쪽 바깥 Shift, 오른쪽 바깥 Shift, 오른쪽 세 번째(바깥쪽) 엄지키입니다.
나머지는 물리적 좌우/행/열 순서에 맞춰 그대로 이식했습니다.

| 행 | 왼손 바깥 → 안쪽 | 오른손 안쪽 → 바깥 |
|---|---|---|
| 위 | V · W · E · R · T | Y · U · I · O · P |
| 가운데 | C · S · D/Shift · F/Shift · G | H · J/Shift · K/Shift · L · ; |
| 아래 | Z · X · A · Q · B | N · M · , · . · Slash/Question |
| 엄지 | Ctrl · Alt · Space/NAV | Space/Shift · Right Alt |

위 표는 기본 입력 요약입니다. Alt 숫자 변환, Ctrl 예외, Ctrl+G 변환,
콤마/마침표 및 기타 실제 동작은 원본의 커스텀 behavior를 그대로 유지합니다.
Space/NAV는 탭 Space·홀드 NAV, 오른손 Space/Shift는 탭 Space·홀드 Shift입니다.
오른손 아래 바깥키는 원본대로 탭 Slash·홀드 Question입니다.

## 레이어와 콤보

- 토템에서 이식한 QWERTY / NAV / NUM / OSL은 4개 레이어 × 35키다.
  현재 구성에는 [Mouse/Scroll/Snipe](trackball-modes.md)를 추가해 총 7개 레이어를 사용한다.
- 원본 콤보 46개, 매크로·탭댄스·mod-morph 및 시간 설정을 보존했습니다.
- 콤보의 key-positions와 홈로우 Shift의 hold-trigger-key-positions는 새 번호로 다시 계산했습니다.
- NAV: 왼손 Space 엄지 홀드.
- NUM: 기본 레이어의 **S+Q** 콤보. 원본 탭댄스대로 누르는 동안 활성화하며 더블탭으로 토글합니다.
  NUM에서도 같은 S+Q 위치 콤보로 기본 레이어로 돌아옵니다.
- OSL: 기본 레이어의 **S+D+F** 콤보로 일회성 활성화.
- 제거한 오른손 세 번째 엄지키의 원래 기능은 다른 키에 임의로 옮기지 않았습니다.
- 토템 원본에는 마우스 클릭 바인딩이 없습니다. 현재는 후속 Mouse/Scroll/Snipe 레이어로 클릭과 모드 전환을 지원합니다.
- Nano용 서비스 조합으로 OSL에서 양쪽 위 바깥키(V+P 위치)를 함께 누르면 Studio unlock입니다.
  Studio 기능은 Studio 빌드에서 사용합니다. 기존 NAV+C unlock 설명은 이전 키맵 기록입니다.

토템의 Bluetooth 프로필 0/1/2 선택키가 유효하도록 동글의 저장 본드 수를 5로 설정했습니다.
이는 좌우 split 2개 + 호스트 프로필 3개입니다. 최대 동시 연결 수는 3, USB 우선 출력은 유지합니다.
OLED 드라이버·배선·메모리·작업 큐·레이아웃·봉고캣은 변경하지 않았습니다.
현재 OLED는 QWERTY/NAV/NUM/OSL/MOUSE/SCROLL/SNIPE 이름을 표시합니다.

## 설치와 확인

새 OLED 동글 UF2만 동글에 설치하면 됩니다. 일반 키맵 변경이므로 좌우 재설치나 본드 초기화는 필요하지 않습니다.
기존 정상 OLED UF2는 복구용으로 보존했습니다. Studio UF2는 별도의 Studio 전용 선택 빌드입니다.

물리 키 35개, 양손 엄지 순서, S+Q/ S+D+F, D/F/J/K 홀드 Shift, Alt 숫자,
키 입력과 트랙볼 동시 사용을 실기기에서 확인하세요. 빌드/매핑 검사 성공은 실기기 확인과 구분합니다.

검증 스크립트 `scripts/test-keymap-port.py`는 원본 토템 파일과 직접 비교해 140개 바인딩,
46개 콤보, 커스텀 behavior/시간/매크로 및 홈로우 위치를 검사합니다. 생성 DTS에서도
모든 키보드 빌드의 토템 4×35 바인딩과 재매핑된 위치를 확인합니다. 후속 3개 모드는 별도 검사합니다.
