# GitHub에 처음 올리기

1. GitHub에서 **새 빈 저장소**를 만듭니다. 이름 예: `zmk-charybdis-nano-dongle`.
   공개/비공개는 원하는 대로 선택하고 README·.gitignore·라이선스 자동 생성은 선택하지 않습니다.
2. PowerShell에서 이 프로젝트 폴더로 이동합니다. 아래 URL은 실제 새 저장소 URL로 바꿉니다.

```powershell
git init -b main
git add .
git status
git commit -m "Initial Nano35 dongle configuration"
git remote add origin https://github.com/YOUR-NAME/YOUR-REPO.git
git push -u origin main
```

Git이 작성자 정보를 요구하면 본인의 이름과 GitHub 이메일(또는 GitHub 제공 noreply 주소)을
`git config user.name`, `git config user.email`로 이 저장소에 설정한 뒤 commit을 다시 실행합니다.
로그인 창이 나오면 본인 GitHub 계정으로 로그인합니다.
기존 다른 키보드 저장소 URL을 origin으로 지정하지 마세요.

3. GitHub **Actions**에서 펌웨어 빌드와 **Draw ZMK keymap** 실행 결과를 확인합니다.
   6개 UF2와 키맵 그림이 각각 Artifacts에 올라옵니다. 그림은 기본 브랜치에 자동 커밋됩니다.
   저장소/조직 정책이 Actions 쓰기를 금지하거나 브랜치 보호로 직접 push를 막으면 그림 자동 커밋은
   실패할 수 있습니다. 이 경우 Artifacts 그림을 받거나 로컬에서 생성 후 직접 commit합니다.

## 이후 수정

```powershell
git pull --ff-only
# config/nano35.keymap 등을 편집
git add .
git commit -m "Update keymap"
git push
```

그림 자동 커밋이 생기므로 다음 편집 전에 pull합니다. PR에서는 그림 생성/검사와 artifact 업로드만 하고
기본 브랜치 갱신은 하지 않습니다. `keymap_drawer.config.yaml`은 표시 이름/스타일을 바꾸는 곳입니다.
생성된 SVG/YAML을 직접 수정하면 다음 생성 시 덮어씁니다.

## 올리는 것과 로컬 보관

소스·문서·워크플로·키맵 그림만 올립니다. `.gitignore`가 `artifacts/`, `설치파일/`, `build/`,
`docs/archive/`를 제외합니다. 과거 UF2와 로그는 로컬에서 그대로 사용할 수 있습니다.
소스 백업 ZIP을 사용할 경우 압축을 풀고 프로젝트 루트에서 위 명령을 실행합니다.
GitHub Actions 실행 성공은 업로드 후 별도로 확인해야 합니다.
