# 로컬 빌드

Python 3.12, `requirements-build.txt`, Zephyr SDK 0.16.8의 ARM 툴체인을 사용합니다.
west 의존성은 `config/west.lock.yml`로 고정합니다.
최초 의존성 설정과 Linux 빌드 절차는 `.github/workflows/build.yml`을 참고하세요.

Windows에서 준비된 west 작업 공간·Python 환경·SDK 경로를 넘깁니다.

```powershell
.\scripts\build-local.ps1 -Workspace C:\zmk-work -Python C:\zmk-tools\venv\Scripts\python.exe -Sdk C:\zmk-tools\zephyr-sdk-0.16.8 -Target 4
```

위 경로는 예시이며 실제 설치 위치로 바꿉니다. Target 0=동글, 1=왼손, 2=오른손, 3=초기화,
4=OLED, 5=Studio입니다. 생략하면 전체를 빌드합니다. 출력은 `artifacts/`입니다.
소스 경로에 공백이 있으면 실행기가 공백 없는 작업 공간에 junction을 만들어 참조합니다.

전체 빌드 후 `python scripts/verify-builds.py --workspace C:\zmk-work`로 생성 설정을 검사합니다.
`test-trackball-modes.py`, `test-display.py`의 ARM 실행에는 SDK의 `--arm-gcc`와
`unicorn==2.1.4`, pyelftools가 필요합니다. 원본 키맵 대조 스크립트에는 별도 참조 키맵 경로를 넘깁니다.
각 스크립트의 `--help`에서 정확한 인자를 확인하세요.

## 그림만 갱신

```powershell
python -m pip install -r requirements-draw.txt
python scripts/draw-keymap.py
```

펌웨어 도구체인 없이 실행 가능합니다. 7개 레이어 × 35키와 콤보 위치 범위를 검사하고
전체/기본/콤보 SVG 및 파싱 YAML을 만듭니다.
